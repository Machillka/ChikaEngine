#include "ChikaEngine/RenderGraph.hpp"

#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace ChikaEngine::Render
{
    namespace
    {
        /**
         * @brief 组合资源描述字段，生成仅用于同类型临时资源复用的稳定 Hash。
         */
        void HashCombine(uint64_t& seed, uint64_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
        }

        /**
         * @brief 把依赖插入 Pass，同时避免自依赖和重复边。
         */
        void AddDependency(RGPass& pass, RGPassHandle dependency)
        {
            if (!dependency.IsValid() || dependency == pass.handle)
                return;
            if (std::ranges::find(pass.dependencies, dependency) == pass.dependencies.end())
                pass.dependencies.push_back(dependency);
        }

        /**
         * @brief 返回便于 Graph Dump 阅读的 Pass 类型名称。
         */
        const char* PassTypeName(RGPassType type)
        {
            switch (type)
            {
            case RGPassType::Graphics:
                return "Graphics";
            case RGPassType::Compute:
                return "Compute";
            case RGPassType::Copy:
                return "Copy";
            case RGPassType::Present:
                return "Present";
            }
            return "Unknown";
        }
    } // namespace

    RenderGraph::RenderGraph(IRHIDevice* device) : m_device(device) {}

    RenderGraph::~RenderGraph()
    {
        for (auto& [hash, pool] : m_physicalTexturePool)
        {
            for (const TexturePoolEntry& texture : pool)
                m_device->DestroyTexture(texture.handle);
        }
        for (auto& [hash, pool] : m_physicalBufferPool)
        {
            for (const BufferHandle buffer : pool)
                m_device->DestroyBuffer(buffer);
        }
    }

    /**
     * @brief 计算 Texture 物理兼容性 Hash，防止错误复用不同 mip/layer/sample 资源。
     */
    uint64_t RenderGraph::ComputeTextureHash(const TextureDesc& desc)
    {
        uint64_t hash = 0;
        HashCombine(hash, desc.width);
        HashCombine(hash, desc.height);
        HashCombine(hash, static_cast<uint64_t>(desc.format));
        HashCombine(hash, desc.mipLevels);
        HashCombine(hash, desc.arrayLayers);
        HashCombine(hash, desc.sampleCount);
        HashCombine(hash, static_cast<uint64_t>(desc.usage));
        return hash;
    }

    /**
     * @brief 计算 Buffer 物理兼容性 Hash；大小和用途均必须兼容。
     */
    uint64_t RenderGraph::ComputeBufferHash(const BufferDesc& desc)
    {
        uint64_t hash = 0;
        HashCombine(hash, desc.size);
        HashCombine(hash, static_cast<uint64_t>(desc.usage));
        HashCombine(hash, static_cast<uint64_t>(desc.memoryUsage));
        return hash;
    }

    RGTextureHandle RenderGraph::ImportTexture(const std::string& name, TextureHandle handle, const TextureDesc& desc, ResourceState initialState, ResourceState finalState)
    {
        return m_textures.Create({ name, desc, handle, true, initialState, finalState });
    }

    RGBufferHandle RenderGraph::ImportBuffer(const std::string& name, BufferHandle handle, const BufferDesc& desc, ResourceState initialState, ResourceState finalState)
    {
        return m_buffers.Create({ name, desc, handle, true, initialState, finalState });
    }

    void RenderGraph::UpdateImportedTexture(RGTextureHandle handle, TextureHandle newHandle)
    {
        if (RGTextureResource* texture = m_textures.Get(handle))
            texture->physicalHandle = newHandle;
    }

    RGTextureHandle RenderGraph::_RegisterTexture(const std::string& name, const TextureDesc& desc)
    {
        return m_textures.Create({ name, desc });
    }

    RGBufferHandle RenderGraph::_RegisterBuffer(const std::string& name, const BufferDesc& desc)
    {
        return m_buffers.Create({ name, desc });
    }

    TextureHandle RenderGraph::GetPhysicalTexture(RGTextureHandle handle)
    {
        const RGTextureResource* texture = m_textures.Get(handle);
        return texture ? texture->physicalHandle : TextureHandle::Invalid();
    }

    BufferHandle RenderGraph::GetPhysicalBuffer(RGBufferHandle handle)
    {
        const RGBufferResource* buffer = m_buffers.Get(handle);
        return buffer ? buffer->physicalHandle : BufferHandle::Invalid();
    }

    const TextureDesc& RenderGraph::GetTextureDesc(RGTextureHandle handle)
    {
        return m_textures.Get(handle)->desc;
    }

    const BufferDesc& RenderGraph::GetBufferDesc(RGBufferHandle handle)
    {
        return m_buffers.Get(handle)->desc;
    }

    /**
     * @brief 创建指定类型 Pass，并在 Setup 阶段收集全部资源契约。
     */
    void RenderGraph::AddTypedPass(const std::string& name, RGPassType type, bool hasSideEffects, RGSetupCallback setup, RGExecuteCallback execute)
    {
        RGPass pass;
        pass.name = name;
        pass.type = type;
        pass.hasSideEffects = hasSideEffects;

        const RGPassHandle handle = m_passes.Create(pass);
        RGPass* storedPass = m_passes.Get(handle);
        storedPass->handle = handle;
        RGPassBuilder builder(storedPass, this);
        if (setup)
            setup(builder);
        storedPass->executeCallback = std::move(execute);
        m_passInsertionOrder.push_back(handle);
    }

    void RenderGraph::AddPass(const std::string& name, RGSetupCallback setup, RGExecuteCallback execute)
    {
        AddTypedPass(name, RGPassType::Graphics, false, std::move(setup), std::move(execute));
    }

    void RenderGraph::AddComputePass(const std::string& name, RGSetupCallback setup, RGExecuteCallback execute)
    {
        AddTypedPass(name, RGPassType::Compute, false, std::move(setup), std::move(execute));
    }

    void RenderGraph::AddCopyPass(const std::string& name, RGSetupCallback setup, RGExecuteCallback execute)
    {
        AddTypedPass(name, RGPassType::Copy, true, std::move(setup), std::move(execute));
    }

    void RenderGraph::AddUploadPass(const std::string& name, RGExecuteCallback execute)
    {
        AddCopyPass(name, {}, std::move(execute));
    }

    void RenderGraph::AddPresentPass(const std::string& name, RGTextureHandle swapchainHandle)
    {
        AddTypedPass(name, RGPassType::Present, true, [swapchainHandle](RGPassBuilder& builder) { builder.ReadTexture(swapchainHandle, ResourceState::Present); }, {});
    }

    /**
     * @brief 验证声明并按插入顺序构建 RAW/WAR/WAW 依赖，再剔除不可达 Pass。
     */
    bool RenderGraph::Compile()
    {
        m_sortedPasses.clear();
        m_compileErrors.clear();
        m_compileSucceeded = false;

        m_textures.ForEach(
            [](RGTextureHandle, RGTextureResource& resource)
            {
                resource.refCount = 0;
                resource.firstUsePassIdx = UINT32_MAX;
                resource.lastUsePassIdx = UINT32_MAX;
            });
        m_buffers.ForEach(
            [](RGBufferHandle, RGBufferResource& resource)
            {
                resource.refCount = 0;
                resource.firstUsePassIdx = UINT32_MAX;
                resource.lastUsePassIdx = UINT32_MAX;
            });
        m_passes.ForEach(
            [](RGPassHandle, RGPass& pass)
            {
                pass.dependencies.clear();
                pass.refCount = 0;
                pass.isCulled = true;
            });

        std::unordered_map<RGTextureHandle, RGPassHandle> lastTextureWriter;
        std::unordered_map<RGBufferHandle, RGPassHandle> lastBufferWriter;
        std::unordered_map<RGTextureHandle, std::vector<RGPassHandle>> textureReaders;
        std::unordered_map<RGBufferHandle, std::vector<RGPassHandle>> bufferReaders;

        const auto fail = [&](const RGPass& pass, const std::string& message) { m_compileErrors.push_back(pass.name + ": " + message); };

        for (const RGPassHandle passHandle : m_passInsertionOrder)
        {
            RGPass* pass = m_passes.Get(passHandle);
            if (!pass)
                continue;

            if (pass->type != RGPassType::Graphics && (!pass->colorWrites.empty() || pass->hasDepth))
                fail(*pass, "non-graphics pass declares an attachment");
            if (pass->type == RGPassType::Graphics && pass->colorWrites.empty() && !pass->hasDepth)
                fail(*pass, "graphics pass has no color or depth attachment");

            for (const RGTextureUsage& read : pass->textureReads)
            {
                const RGTextureResource* resource = m_textures.Get(read.handle);
                if (!resource)
                {
                    fail(*pass, "reads an invalid texture handle");
                    continue;
                }
                const auto writer = lastTextureWriter.find(read.handle);
                if (writer != lastTextureWriter.end())
                    AddDependency(*pass, writer->second);
                else if (!resource->isImported)
                    fail(*pass, "reads transient texture '" + resource->name + "' before its producer");
                textureReaders[read.handle].push_back(passHandle);
            }

            const auto processTextureWrite = [&](const RGTextureUsage& write)
            {
                const RGTextureResource* resource = m_textures.Get(write.handle);
                if (!resource)
                {
                    fail(*pass, "writes an invalid texture handle");
                    return;
                }
                const auto writer = lastTextureWriter.find(write.handle);
                if (writer != lastTextureWriter.end())
                {
                    if (!resource->isImported)
                        fail(*pass, "adds a duplicate producer for transient texture '" + resource->name + "'");
                    AddDependency(*pass, writer->second);
                }
                for (const RGPassHandle reader : textureReaders[write.handle])
                    AddDependency(*pass, reader);
                textureReaders[write.handle].clear();
                lastTextureWriter[write.handle] = passHandle;
            };
            for (const RGTextureUsage& write : pass->textureWrites)
                processTextureWrite(write);
            for (const RGTextureUsage& write : pass->colorWrites)
                processTextureWrite(write);
            if (pass->hasDepth)
                processTextureWrite(pass->depthWrite);

            for (const RGBufferUsage& read : pass->bufferReads)
            {
                const RGBufferResource* resource = m_buffers.Get(read.handle);
                if (!resource)
                {
                    fail(*pass, "reads an invalid buffer handle");
                    continue;
                }
                const auto writer = lastBufferWriter.find(read.handle);
                if (writer != lastBufferWriter.end())
                    AddDependency(*pass, writer->second);
                else if (!resource->isImported)
                    fail(*pass, "reads transient buffer '" + resource->name + "' before its producer");
                bufferReaders[read.handle].push_back(passHandle);
            }

            for (const RGBufferUsage& write : pass->bufferWrites)
            {
                const RGBufferResource* resource = m_buffers.Get(write.handle);
                if (!resource)
                {
                    fail(*pass, "writes an invalid buffer handle");
                    continue;
                }
                const auto writer = lastBufferWriter.find(write.handle);
                if (writer != lastBufferWriter.end())
                {
                    if (!resource->isImported)
                        fail(*pass, "adds a duplicate producer for transient buffer '" + resource->name + "'");
                    AddDependency(*pass, writer->second);
                }
                for (const RGPassHandle reader : bufferReaders[write.handle])
                    AddDependency(*pass, reader);
                bufferReaders[write.handle].clear();
                lastBufferWriter[write.handle] = passHandle;
            }
        }

        if (!m_compileErrors.empty())
        {
            BuildDebugSnapshot();
            for (const std::string& error : m_compileErrors)
                LOG_ERROR("RenderGraph", "{}", error);
            return false;
        }

        std::unordered_set<uint32_t> livePasses;
        const std::function<void(RGPassHandle)> markLive = [&](RGPassHandle handle)
        {
            if (!handle.IsValid() || livePasses.contains(handle.raw_value))
                return;
            RGPass* pass = m_passes.Get(handle);
            if (!pass)
                return;
            livePasses.insert(handle.raw_value);
            pass->isCulled = false;
            pass->refCount = 1;
            for (const RGPassHandle dependency : pass->dependencies)
                markLive(dependency);
        };
        for (const RGPassHandle handle : m_passInsertionOrder)
        {
            const RGPass* pass = m_passes.Get(handle);
            if (pass && pass->hasSideEffects)
                markLive(handle);
        }

        std::unordered_set<uint32_t> visiting;
        std::unordered_set<uint32_t> visited;
        const std::function<void(RGPassHandle)> schedule = [&](RGPassHandle handle)
        {
            if (!handle.IsValid() || visited.contains(handle.raw_value))
                return;
            if (visiting.contains(handle.raw_value))
            {
                const RGPass* pass = m_passes.Get(handle);
                m_compileErrors.push_back((pass ? pass->name : "Unknown") + std::string(": dependency cycle detected"));
                return;
            }
            RGPass* pass = m_passes.Get(handle);
            if (!pass || pass->isCulled)
                return;
            visiting.insert(handle.raw_value);
            for (const RGPassHandle dependency : pass->dependencies)
                schedule(dependency);
            visiting.erase(handle.raw_value);
            visited.insert(handle.raw_value);
            m_sortedPasses.push_back(handle);
        };
        for (const RGPassHandle handle : m_passInsertionOrder)
            schedule(handle);

        for (uint32_t passIndex = 0; passIndex < m_sortedPasses.size(); ++passIndex)
        {
            RGPass* pass = m_passes.Get(m_sortedPasses[passIndex]);
            const auto updateTexture = [&](RGTextureHandle handle)
            {
                if (RGTextureResource* resource = m_textures.Get(handle))
                {
                    resource->firstUsePassIdx = std::min(resource->firstUsePassIdx, passIndex);
                    resource->lastUsePassIdx = passIndex;
                    ++resource->refCount;
                }
            };
            const auto updateBuffer = [&](RGBufferHandle handle)
            {
                if (RGBufferResource* resource = m_buffers.Get(handle))
                {
                    resource->firstUsePassIdx = std::min(resource->firstUsePassIdx, passIndex);
                    resource->lastUsePassIdx = passIndex;
                    ++resource->refCount;
                }
            };
            for (const RGTextureUsage& usage : pass->textureReads)
                updateTexture(usage.handle);
            for (const RGTextureUsage& usage : pass->textureWrites)
                updateTexture(usage.handle);
            for (const RGTextureUsage& usage : pass->colorWrites)
                updateTexture(usage.handle);
            if (pass->hasDepth)
                updateTexture(pass->depthWrite.handle);
            for (const RGBufferUsage& usage : pass->bufferReads)
                updateBuffer(usage.handle);
            for (const RGBufferUsage& usage : pass->bufferWrites)
                updateBuffer(usage.handle);
        }

        m_compileSucceeded = m_compileErrors.empty();
        BuildDebugSnapshot();
        return m_compileSucceeded;
    }

    TextureHandle RenderGraph::AllocatePhysicalTexture(const TextureDesc& desc)
    {
        auto& pool = m_physicalTexturePool[ComputeTextureHash(desc)];
        if (!pool.empty())
        {
            const TextureHandle handle = pool.back().handle;
            pool.pop_back();
            return handle;
        }
        return m_device->CreateTexture(desc);
    }

    BufferHandle RenderGraph::AllocatePhysicalBuffer(const BufferDesc& desc)
    {
        auto& pool = m_physicalBufferPool[ComputeBufferHash(desc)];
        if (!pool.empty())
        {
            const BufferHandle handle = pool.back();
            pool.pop_back();
            return handle;
        }
        return m_device->CreateBuffer(desc);
    }

    void RenderGraph::ReleasePhysicalTexture(TextureHandle handle, const TextureDesc& desc)
    {
        m_physicalTexturePool[ComputeTextureHash(desc)].push_back({ handle, 0 });
    }

    void RenderGraph::ReleasePhysicalBuffer(BufferHandle handle, const BufferDesc& desc)
    {
        m_physicalBufferPool[ComputeBufferHash(desc)].push_back(handle);
    }

    /**
     * @brief 执行编译后的 Pass，并记录实际 Barrier 与每 Pass CPU 录制耗时。
     */
    void RenderGraph::Execute()
    {
        m_lastExecutedPassCount = 0;
        m_debugSnapshot.barriers.clear();
        if (!m_compileSucceeded || m_sortedPasses.empty())
            return;

        m_textureStateMap.clear();
        m_bufferStateMap.clear();
        m_textures.ForEach(
            [&](RGTextureHandle handle, RGTextureResource& resource)
            {
                if (resource.isImported)
                    m_textureStateMap[handle] = resource.initialState;
            });
        m_buffers.ForEach(
            [&](RGBufferHandle handle, RGBufferResource& resource)
            {
                if (resource.isImported)
                    m_bufferStateMap[handle] = resource.initialState;
            });

        IRHICommandList* commandList = m_device->AllocateCommandList();
        commandList->Begin();
        commandList->SetDebugName("RenderGraph.Frame");

        for (uint32_t passIndex = 0; passIndex < m_sortedPasses.size(); ++passIndex)
        {
            RGPass* pass = m_passes.Get(m_sortedPasses[passIndex]);
            const float labelColor[4] = { 0.25f, 0.55f, 0.95f, 1.0f };
            commandList->BeginDebugLabel(pass->name, labelColor);
            commandList->BeginTimestampScope(pass->name);

            m_textures.ForEach(
                [&](RGTextureHandle handle, RGTextureResource& resource)
                {
                    if (!resource.isImported && resource.refCount > 0 && resource.firstUsePassIdx == passIndex)
                    {
                        resource.physicalHandle = AllocatePhysicalTexture(resource.desc);
                        m_textureStateMap[handle] = ResourceState::Undefined;
                    }
                });
            m_buffers.ForEach(
                [&](RGBufferHandle handle, RGBufferResource& resource)
                {
                    if (!resource.isImported && resource.refCount > 0 && resource.firstUsePassIdx == passIndex)
                    {
                        resource.physicalHandle = AllocatePhysicalBuffer(resource.desc);
                        m_bufferStateMap[handle] = ResourceState::Undefined;
                    }
                });

            ApplyBarriers(pass, commandList);
            const auto cpuStart = std::chrono::steady_clock::now();
            if (pass->type == RGPassType::Graphics)
            {
                std::vector<RenderingAttachment> colors;
                for (const RGTextureUsage& write : pass->colorWrites)
                {
                    RenderingAttachment attachment;
                    attachment.texture = GetPhysicalTexture(write.handle);
                    attachment.loadOp = write.loadOp;
                    attachment.storeOp = write.storeOp;
                    std::ranges::copy(write.clearColor, attachment.clearColor);
                    colors.push_back(attachment);
                }
                RenderingAttachment depth;
                if (pass->hasDepth)
                {
                    depth.texture = GetPhysicalTexture(pass->depthWrite.handle);
                    depth.loadOp = pass->depthWrite.loadOp;
                    depth.storeOp = pass->depthWrite.storeOp;
                }
                commandList->BeginRendering(colors, pass->hasDepth ? &depth : nullptr);
                if (pass->executeCallback)
                    pass->executeCallback(commandList, this);
                commandList->EndRendering();
            }
            else if (pass->executeCallback)
            {
                pass->executeCallback(commandList, this);
            }
            pass->lastCpuTimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpuStart).count();

            ApplyFinalBarriers(passIndex, commandList);
            m_textures.ForEach(
                [&](RGTextureHandle handle, RGTextureResource& resource)
                {
                    if (!resource.isImported && resource.refCount > 0 && resource.lastUsePassIdx == passIndex)
                    {
                        ReleasePhysicalTexture(resource.physicalHandle, resource.desc);
                        resource.physicalHandle = TextureHandle::Invalid();
                    }
                });
            m_buffers.ForEach(
                [&](RGBufferHandle handle, RGBufferResource& resource)
                {
                    if (!resource.isImported && resource.refCount > 0 && resource.lastUsePassIdx == passIndex)
                    {
                        ReleasePhysicalBuffer(resource.physicalHandle, resource.desc);
                        resource.physicalHandle = BufferHandle::Invalid();
                    }
                });

            commandList->EndDebugLabel();
            commandList->EndTimestampScope();
            ++m_lastExecutedPassCount;
        }
        commandList->End();
        m_device->Submit(commandList);
        BuildDebugSnapshot();
    }

    std::vector<std::string> RenderGraph::GetCompiledPassNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_sortedPasses.size());
        for (const RGPassHandle handle : m_sortedPasses)
        {
            if (const RGPass* pass = m_passes.Get(handle))
                names.push_back(pass->name);
        }
        return names;
    }

    /**
     * @brief 根据 Pass 声明自动插入 Texture 与 Buffer Barrier。
     */
    void RenderGraph::ApplyBarriers(RGPass* pass, IRHICommandList* commandList)
    {
        const auto transitionTexture = [&](const RGTextureUsage& usage)
        {
            ResourceState& current = m_textureStateMap[usage.handle];
            if (current == usage.state)
                return;
            const RGTextureResource* resource = m_textures.Get(usage.handle);
            commandList->InsertTextureBarrier(GetPhysicalTexture(usage.handle), current, usage.state, usage.range);
            m_debugSnapshot.barriers.push_back({ pass->name, resource ? resource->name : "InvalidTexture", current, usage.state });
            current = usage.state;
        };
        const auto transitionBuffer = [&](const RGBufferUsage& usage)
        {
            ResourceState& current = m_bufferStateMap[usage.handle];
            if (current == usage.state)
                return;
            const RGBufferResource* resource = m_buffers.Get(usage.handle);
            commandList->InsertBufferBarrier(GetPhysicalBuffer(usage.handle), current, usage.state, usage.range);
            m_debugSnapshot.barriers.push_back({ pass->name, resource ? resource->name : "InvalidBuffer", current, usage.state });
            current = usage.state;
        };
        for (const RGTextureUsage& usage : pass->textureReads)
            transitionTexture(usage);
        for (const RGTextureUsage& usage : pass->textureWrites)
            transitionTexture(usage);
        for (const RGTextureUsage& usage : pass->colorWrites)
            transitionTexture(usage);
        if (pass->hasDepth)
            transitionTexture(pass->depthWrite);
        for (const RGBufferUsage& usage : pass->bufferReads)
            transitionBuffer(usage);
        for (const RGBufferUsage& usage : pass->bufferWrites)
            transitionBuffer(usage);
    }

    /**
     * @brief 在 Imported Resource 最后一次使用后转换到调用者要求的 final state。
     */
    void RenderGraph::ApplyFinalBarriers(uint32_t passIndex, IRHICommandList* commandList)
    {
        m_textures.ForEach(
            [&](RGTextureHandle handle, RGTextureResource& resource)
            {
                if (!resource.isImported || resource.lastUsePassIdx != passIndex || resource.finalState == ResourceState::Undefined)
                    return;
                ResourceState& current = m_textureStateMap[handle];
                if (current != resource.finalState)
                {
                    commandList->InsertTextureBarrier(resource.physicalHandle, current, resource.finalState);
                    m_debugSnapshot.barriers.push_back({ "FinalState", resource.name, current, resource.finalState });
                    current = resource.finalState;
                }
            });
        m_buffers.ForEach(
            [&](RGBufferHandle handle, RGBufferResource& resource)
            {
                if (!resource.isImported || resource.lastUsePassIdx != passIndex || resource.finalState == ResourceState::Undefined)
                    return;
                ResourceState& current = m_bufferStateMap[handle];
                if (current != resource.finalState)
                {
                    commandList->InsertBufferBarrier(resource.physicalHandle, current, resource.finalState);
                    m_debugSnapshot.barriers.push_back({ "FinalState", resource.name, current, resource.finalState });
                    current = resource.finalState;
                }
            });
    }

    /**
     * @brief 构建编辑器和测试可消费的 DAG、生命周期与 Barrier 文本快照。
     */
    void RenderGraph::BuildDebugSnapshot()
    {
        m_debugSnapshot.passes.clear();
        m_debugSnapshot.resources.clear();
        m_debugSnapshot.compileErrors = m_compileErrors;
        std::unordered_map<uint32_t, std::string> passNames;
        std::unordered_map<std::string, double> gpuTimes;
        for (const RenderPassGpuTiming& timing : m_device->GetPassGpuTimings())
            gpuTimes[timing.name] = timing.gpuTimeMs;
        m_passes.ForEach([&](RGPassHandle handle, const RGPass& pass) { passNames[handle.raw_value] = pass.name; });
        for (const RGPassHandle handle : m_sortedPasses)
        {
            const RGPass* pass = m_passes.Get(handle);
            if (!pass)
                continue;
            RenderGraphPassDebugInfo info;
            info.name = pass->name;
            info.type = pass->type;
            info.cpuTimeMs = pass->lastCpuTimeMs;
            info.gpuTimeMs = gpuTimes[pass->name];
            for (const RGPassHandle dependency : pass->dependencies)
                info.dependencies.push_back(passNames[dependency.raw_value]);
            m_debugSnapshot.passes.push_back(std::move(info));
        }
        m_textures.ForEach([&](RGTextureHandle, const RGTextureResource& resource) { m_debugSnapshot.resources.push_back({ resource.name, "Texture", resource.firstUsePassIdx, resource.lastUsePassIdx, resource.isImported }); });
        m_buffers.ForEach([&](RGBufferHandle, const RGBufferResource& resource) { m_debugSnapshot.resources.push_back({ resource.name, "Buffer", resource.firstUsePassIdx, resource.lastUsePassIdx, resource.isImported }); });

        std::ostringstream dump;
        dump << "RenderGraph\n";
        for (const RenderGraphPassDebugInfo& pass : m_debugSnapshot.passes)
        {
            dump << "  [" << PassTypeName(pass.type) << "] " << pass.name << " <-";
            for (const std::string& dependency : pass.dependencies)
                dump << ' ' << dependency;
            dump << '\n';
        }
        dump << "Resources\n";
        for (const RenderGraphResourceDebugInfo& resource : m_debugSnapshot.resources)
            dump << "  [" << resource.kind << "] " << resource.name << " lifetime=" << resource.firstUsePass << ".." << resource.lastUsePass << (resource.imported ? " imported" : " transient") << '\n';
        if (!m_compileErrors.empty())
        {
            dump << "Errors\n";
            for (const std::string& error : m_compileErrors)
                dump << "  " << error << '\n';
        }
        m_debugSnapshot.dump = dump.str();
    }

    void RenderGraph::Clear()
    {
        m_passes.Clear();
        m_textures.Clear();
        m_buffers.Clear();
        m_passInsertionOrder.clear();
        m_sortedPasses.clear();
        m_compileErrors.clear();
        m_debugSnapshot = {};
        m_lastExecutedPassCount = 0;
        m_compileSucceeded = false;
    }
} // namespace ChikaEngine::Render
