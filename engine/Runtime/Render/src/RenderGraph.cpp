#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RenderGraphHandle.hpp"
#include "ChikaEngine/RenderGraphPass.hpp"
#include "ChikaEngine/RenderGraphResource.hpp"
#include "ChikaEngine/RenderPassBuilder.hpp"
#include <ChikaEngine/rhi/Vulkan/VulkanRHIDevice.hpp>
#include <cstdint>
#include <vector>
#include "ChikaEngine/IRHICommandList.hpp"

namespace ChikaEngine::Render
{
    inline void hash_combine(uint64_t& seed, uint64_t v)
    {
        seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    // texture -> hash key
    uint64_t RenderGraph::ComputeTextureHash(const TextureDesc& desc)
    {
        uint64_t hash = 0;
        hash_combine(hash, desc.width);
        hash_combine(hash, desc.height);
        hash_combine(hash, static_cast<uint64_t>(desc.format));
        hash_combine(hash, static_cast<uint64_t>(desc.usage));
        return hash;
    }

    RenderGraph::RenderGraph(IRHIDevice* device) : m_device(device) {}

    RenderGraph::~RenderGraph()
    {
        for (auto& [hash, pool] : m_physicalTexturePool)
        {
            for (auto& tex : pool)
            {
                m_device->DestroyTexture(tex.handle);
            }
        }
    }

    RGTextureHandle RenderGraph::ImportTexture(const std::string& name, TextureHandle handle, const TextureDesc& desc)
    {
        return m_textures.Create({
            name,
            desc,
            handle,
            true,
        });
    }

    void RenderGraph::UpdateImportedTexture(RGTextureHandle handle, TextureHandle newHandle)
    {
        auto* tex = m_textures.Get(handle);
        if (!tex)
            return;
        tex->physicalHandle = newHandle;
    }

    RGTextureHandle RenderGraph::_RegisterTexture(const std::string& name, const TextureDesc& desc)
    {
        return m_textures.Create({
            name,
            desc,
            TextureHandle{}, // 默认构造非法
            false,
        });
    }

    TextureHandle RenderGraph::GetPhysicalTexture(RGTextureHandle handle)
    {
        auto tex = m_textures.Get(handle)->physicalHandle;
        if (!tex)
            return TextureHandle{};
        return tex;
    }

    const TextureDesc& RenderGraph::GetTextureDesc(RGTextureHandle handle)
    {
        return m_textures.Get(handle)->desc;
    }

    void RenderGraph::AddPass(const std::string& name, RGSetupCallback setup, RGExecuteCallback execute)
    {
        RGPass pass;
        pass.name = name;
        pass.type = RGPassType::Graphics;
        pass.hasSideEffects = false;

        RGPassHandle h = m_passes.Create(pass);
        RGPass* pPass = m_passes.Get(h);
        pPass->handle = h; // self-point

        RGPassBuilder builder(pPass, this);
        setup(builder);

        pPass->executeCallback = std::move(execute);
        m_passInsertionOrder.push_back(h);
    }

    void RenderGraph::AddUploadPass(const std::string& name, RGExecuteCallback execute)
    {
        RGPass pass;
        pass.name = name;
        pass.type = RGPassType::Upload;
        pass.hasSideEffects = true; // 强制执行

        RGPassHandle h = m_passes.Create(pass);
        RGPass* pPass = m_passes.Get(h);
        pPass->handle = h;
        pPass->executeCallback = std::move(execute);
        m_passInsertionOrder.push_back(h);
    }

    void RenderGraph::AddPresentPass(const std::string& name, RGTextureHandle swapchainHandle)
    {
        RGPass pass;
        pass.name = name;
        pass.type = RGPassType::Present;
        pass.hasSideEffects = true;

        RGResourceUsage usage;
        usage.handle = swapchainHandle;
        usage.state = ResourceState::Present;
        usage.loadOp = LoadOp::DontCare;
        usage.storeOp = StoreOp::Store;
        pass.reads.push_back(usage);

        RGPassHandle h = m_passes.Create(pass);
        RGPass* pPass = m_passes.Get(h);
        pPass->handle = h;
        m_passInsertionOrder.push_back(h);
    }

    void RenderGraph::Compile()
    {
        m_sortedPasses.clear();

        // 重置资源
        m_textures.ForEach(
            [](RGTextureHandle h, RGTextureResource& res)
            {
                res.refCount = 0;
                res.producerPass = RGPassHandle::Invalid();
                res.firstUsePassIdx = UINT32_MAX;
                res.lastUsePassIdx = UINT32_MAX;
            });

        m_passes.ForEach(
            [&](RGPassHandle h, RGPass& pass)
            {
                pass.refCount = 0;

                // 构建 producer
                for (auto& write : pass.writes)
                {
                    auto* res = m_textures.Get(write.handle);
                    if (res)
                    {
                        res->producerPass = h;
                    }
                }

                if (pass.hasDepth)
                {
                    if (auto* res = m_textures.Get(pass.depthWrite.handle))
                        res->producerPass = h;
                }
            });

        std::vector<RGPassHandle> sideEffectStack;
        m_passes.ForEach(
            [&](RGPassHandle h, RGPass& pass)
            {
                if (pass.hasSideEffects)
                {
                    pass.refCount++;
                    sideEffectStack.push_back(h);
                }
            });

        while (!sideEffectStack.empty())
        {
            RGPassHandle passHandle = sideEffectStack.back();
            sideEffectStack.pop_back();

            RGPass* pass = m_passes.Get(passHandle);
            if (!pass)
                continue;

            for (const auto& read : pass->reads)
            {
                RGTextureResource* res = m_textures.Get(read.handle);
                if (!res)
                    continue;

                res->refCount++;

                if (res->producerPass.IsValid())
                {
                    RGPass* producer = m_passes.Get(res->producerPass);
                    if (producer && producer->refCount == 0)
                    {
                        producer->refCount++;
                        sideEffectStack.push_back(res->producerPass);
                    }
                }
            }
        }

        // 统计结果 —— 排除反向 ( 从输出结果出发 ) 不可达的节点
        for (RGPassHandle h : m_passInsertionOrder)
        {
            RGPass* pass = m_passes.Get(h);
            if (pass && pass->refCount > 0)
            {
                m_sortedPasses.push_back(h);
            }
        }

        for (uint32_t i = 0; i < m_sortedPasses.size(); i++)
        {
            RGPass* pass = m_passes.Get(m_sortedPasses[i]);

            auto updateLifetime = [&](RGTextureHandle h)
            {
                RGTextureResource* res = m_textures.Get(h);
                if (res)
                {
                    if (res->firstUsePassIdx == UINT32_MAX)
                        res->firstUsePassIdx = i;
                    res->lastUsePassIdx = i;
                }
            };

            for (const auto& read : pass->reads)
                updateLifetime(read.handle);
            for (const auto& write : pass->writes)
                updateLifetime(write.handle);
            if (pass->hasDepth)
                updateLifetime(pass->depthWrite.handle);
        }
    }

    TextureHandle RenderGraph::AllocatePhysicalTexture(const TextureDesc& desc)
    {
        uint64_t hash = ComputeTextureHash(desc);
        auto& pool = m_physicalTexturePool[hash];
        // 简单内存池实现
        if (!pool.empty())
        {
            TextureHandle h = pool.back().handle;
            pool.pop_back();
            return h;
        }
        return m_device->CreateTexture(desc);
    }

    void RenderGraph::ReleasePhysicalTexture(TextureHandle handle, const TextureDesc& desc)
    {
        uint64_t hash = ComputeTextureHash(desc);
        m_physicalTexturePool[hash].push_back({ handle, 0 });
    }

    void RenderGraph::Execute()
    {
        if (m_sortedPasses.empty())
            return;

        m_resourceStateMap.clear();

        IRHICommandList* cmd = m_device->AllocateCommandList();
        cmd->Begin();

        for (uint32_t passIdx = 0; passIdx < m_sortedPasses.size(); ++passIdx)
        {
            RGPass* pass = m_passes.Get(m_sortedPasses[passIdx]);

            // 分配显存
            m_textures.ForEach(
                [&](RGTextureHandle h, RGTextureResource& res)
                {
                    if (!res.isImported && res.refCount > 0 && res.firstUsePassIdx == passIdx)
                    {
                        res.physicalHandle = AllocatePhysicalTexture(res.desc);
                        m_resourceStateMap[h] = ResourceState::Undefined;
                    }
                });

            ApplyBarriers(pass, cmd);

            if (pass->type == RGPassType::Graphics)
            {
                std::vector<RenderingAttachment> colorAtts;

                for (const auto& write : pass->writes)
                {
                    RenderingAttachment att;
                    att.texture = GetPhysicalTexture(write.handle);
                    att.loadOp = write.loadOp;
                    att.storeOp = write.storeOp;
                    std::copy(std::begin(write.clearColor), std::end(write.clearColor), att.clearColor);
                    colorAtts.push_back(att);
                }
                RenderingAttachment depthAtt;
                if (pass->hasDepth)
                {
                    depthAtt.texture = GetPhysicalTexture(pass->depthWrite.handle);
                    depthAtt.loadOp = pass->depthWrite.loadOp;
                    depthAtt.storeOp = pass->depthWrite.storeOp;
                }

                cmd->BeginRendering(colorAtts, pass->hasDepth ? &depthAtt : nullptr);
                if (pass->executeCallback)
                    pass->executeCallback(cmd, this);
                cmd->EndRendering();
            }
            else
            {
                if (pass->executeCallback)
                    pass->executeCallback(cmd, this);
            }

            m_textures.ForEach(
                [&](RGTextureHandle h, RGTextureResource& res)
                {
                    if (!res.isImported && res.refCount > 0 && res.lastUsePassIdx == passIdx)
                    {
                        ReleasePhysicalTexture(res.physicalHandle, res.desc);
                        res.physicalHandle = TextureHandle::Invalid(); // 安全解绑
                    }
                });
        }

        cmd->End();
        m_device->Submit(cmd);
    }

    void RenderGraph::ApplyBarriers(RGPass* pass, IRHICommandList* cmd)
    {
        auto transition = [&](RGResourceUsage& usage)
        {
            ResourceState& currentState = m_resourceStateMap[usage.handle];
            if (currentState != usage.state)
            {
                cmd->InsertTextureBarrier(GetPhysicalTexture(usage.handle), currentState, usage.state);
                currentState = usage.state;
            }
        };

        for (auto& read : pass->reads)
            transition(read);
        for (auto& write : pass->writes)
            transition(write);
        if (pass->hasDepth)
            transition(pass->depthWrite);
    }

    void RenderGraph::Clear()
    {
        m_passes.Clear();
        m_textures.Clear();
        m_passInsertionOrder.clear();
        m_sortedPasses.clear();
        // m_textures.Clear();
    }

} // namespace ChikaEngine::Render