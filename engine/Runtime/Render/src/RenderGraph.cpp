#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RenderGraphHandle.hpp"
#include "ChikaEngine/RenderGraphPass.hpp"
#include "ChikaEngine/RenderGraphResource.hpp"
#include "ChikaEngine/RenderPassBuilder.hpp"
#include <cstdint>
#include <functional>
#include <unordered_set>
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

        std::unordered_set<uint32_t> livePasses;

        // 重置资源与 pass 编译状态。
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
                pass.isCulled = true;

                // 构建资源 producer。当前图模型中一个资源只允许最后一次写入作为 producer。
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

        std::function<void(RGPassHandle)> markLive = [&](RGPassHandle passHandle)
        {
            if (!passHandle.IsValid() || livePasses.contains(passHandle.raw_value))
                return;

            livePasses.insert(passHandle.raw_value);

            RGPass* pass = m_passes.Get(passHandle);
            if (!pass)
                return;

            pass->isCulled = false;
            pass->refCount = 1;

            for (const auto& read : pass->reads)
            {
                RGTextureResource* res = m_textures.Get(read.handle);
                if (!res)
                    continue;

                res->refCount++;

                if (res->producerPass.IsValid())
                {
                    markLive(res->producerPass);
                }
            }

            if (pass->hasDepth)
            {
                RGTextureResource* res = m_textures.Get(pass->depthWrite.handle);
                if (res)
                    res->refCount++;
            }
        };

        for (RGPassHandle h : m_passInsertionOrder)
        {
            RGPass* pass = m_passes.Get(h);
            if (pass && pass->hasSideEffects)
                markLive(h);
        }

        std::unordered_set<uint32_t> visiting;
        std::unordered_set<uint32_t> visited;

        std::function<void(RGPassHandle)> schedule = [&](RGPassHandle passHandle)
        {
            if (!passHandle.IsValid() || visited.contains(passHandle.raw_value))
                return;
            if (visiting.contains(passHandle.raw_value))
                return;

            RGPass* pass = m_passes.Get(passHandle);
            if (!pass || pass->isCulled)
                return;

            visiting.insert(passHandle.raw_value);

            for (const auto& read : pass->reads)
            {
                RGTextureResource* res = m_textures.Get(read.handle);
                if (res && res->producerPass.IsValid())
                    schedule(res->producerPass);
            }

            visiting.erase(passHandle.raw_value);
            visited.insert(passHandle.raw_value);
            m_sortedPasses.push_back(passHandle);
        };

        for (RGPassHandle h : m_passInsertionOrder)
            schedule(h);

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
        m_lastExecutedPassCount = 0;
        if (m_sortedPasses.empty())
            return;

        m_resourceStateMap.clear();

        IRHICommandList* cmd = m_device->AllocateCommandList();
        cmd->Begin();
        cmd->SetDebugName("RenderGraph.Frame");

        for (uint32_t passIdx = 0; passIdx < m_sortedPasses.size(); ++passIdx)
        {
            RGPass* pass = m_passes.Get(m_sortedPasses[passIdx]);
            const float labelColor[4] = { 0.25f, 0.55f, 0.95f, 1.0f };
            cmd->BeginDebugLabel(pass->name, labelColor);

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

            cmd->EndDebugLabel();
            ++m_lastExecutedPassCount;
        }

        cmd->End();
        m_device->Submit(cmd);
    }

    /**
     * @brief 复制最近一次编译后的 Pass 名称顺序。
     *
     * 返回副本可避免测试或编辑器持有 RenderGraph 内部 Pass 的悬空引用。
     */
    std::vector<std::string> RenderGraph::GetCompiledPassNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_sortedPasses.size());
        for (const RGPassHandle handle : m_sortedPasses)
        {
            const RGPass* pass = m_passes.Get(handle);
            if (pass)
                names.push_back(pass->name);
        }
        return names;
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
        m_lastExecutedPassCount = 0;
        // m_textures.Clear();
    }

} // namespace ChikaEngine::Render
