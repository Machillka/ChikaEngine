/*!
 * @file RenderGraph.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 渲染图的实现 直接硬编码使用 vulkan
 * @version 0.1
 * @date 2026-03-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/RenderGraphPass.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/base/SlotMap.h"
#include "RenderPassBuilder.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ChikaEngine::Render
{
    class IRHICommandList;

    struct RenderGraphPassDebugInfo
    {
        std::string name;
        RGPassType type = RGPassType::Graphics;
        std::vector<std::string> dependencies;
        double cpuTimeMs = 0.0;
        double gpuTimeMs = 0.0;
    };

    struct RenderGraphResourceDebugInfo
    {
        std::string name;
        std::string kind;
        uint32_t firstUsePass = UINT32_MAX;
        uint32_t lastUsePass = UINT32_MAX;
        bool imported = false;
    };

    struct RenderGraphBarrierDebugInfo
    {
        std::string pass;
        std::string resource;
        ResourceState before = ResourceState::Undefined;
        ResourceState after = ResourceState::Undefined;
    };

    /**
     * @brief 保存最近一次编译和执行的只读 RenderGraph 诊断快照。
     */
    struct RenderGraphDebugSnapshot
    {
        std::vector<RenderGraphPassDebugInfo> passes;
        std::vector<RenderGraphResourceDebugInfo> resources;
        std::vector<RenderGraphBarrierDebugInfo> barriers;
        std::vector<std::string> compileErrors;
        std::string dump;
    };

    class RenderGraph
    {
      public:
        RenderGraph(IRHIDevice* device);
        ~RenderGraph();

        /**
         * @brief 导入由外部系统拥有的 Texture，并声明图执行前后的状态。
         */
        RGTextureHandle ImportTexture(const std::string& name, TextureHandle handle, const TextureDesc& desc, ResourceState initialState = ResourceState::Undefined, ResourceState finalState = ResourceState::Undefined);
        /**
         * @brief 导入由外部系统拥有的 Buffer，并声明图执行前后的状态。
         */
        RGBufferHandle ImportBuffer(const std::string& name, BufferHandle handle, const BufferDesc& desc, ResourceState initialState = ResourceState::Undefined, ResourceState finalState = ResourceState::Undefined);
        void UpdateImportedTexture(RGTextureHandle handle, TextureHandle newHandle);

        void AddPass(const std::string& name, RGSetupCallback setup, RGExecuteCallback execute);
        void AddComputePass(const std::string& name, RGSetupCallback setup, RGExecuteCallback execute);
        void AddCopyPass(const std::string& name, RGSetupCallback setup, RGExecuteCallback execute, bool hasSideEffects = true);
        /** @brief 兼容旧调用；无声明 Upload 会作为有副作用 Copy Pass 执行。 */
        void AddUploadPass(const std::string& name, RGExecuteCallback execute);
        void AddPresentPass(const std::string& name, RGTextureHandle swapchainHandle);

        /**
         * @brief 验证资源声明、建立有向依赖并生成稳定执行顺序。
         *
         * 返回 false 时 Execute 不会提交命令，错误可通过 GetCompileErrors 查询。
         */
        bool Compile();
        void Execute();

        /**
         * @brief 返回最近一次 Compile 生成的实际执行顺序。
         *
         * 该接口用于自动测试和调试工具验证 Pass 剔除与依赖调度，不暴露内部 Pass 所有权。
         */
        std::vector<std::string> GetCompiledPassNames() const;
        const std::vector<std::string>& GetCompileErrors() const
        {
            return m_compileErrors;
        }
        const RenderGraphDebugSnapshot& GetDebugSnapshot() const
        {
            return m_debugSnapshot;
        }

        /**
         * @brief 返回最近一次 Execute 实际执行的 Pass 数量。
         *
         * Renderer 使用该值补充 RHI 命令统计，因为 RenderGraph 才是 Pass 数量的权威来源。
         */
        uint32_t GetLastExecutedPassCount() const
        {
            return m_lastExecutedPassCount;
        }

        void Clear();

        RGTextureHandle _RegisterTexture(const std::string& name, const TextureDesc& desc);
        RGBufferHandle _RegisterBuffer(const std::string& name, const BufferDesc& desc);
        TextureHandle GetPhysicalTexture(RGTextureHandle handle);
        BufferHandle GetPhysicalBuffer(RGBufferHandle handle);
        const TextureDesc& GetTextureDesc(RGTextureHandle handle);
        const BufferDesc& GetBufferDesc(RGBufferHandle handle);

      private:
        void AddTypedPass(const std::string& name, RGPassType type, bool hasSideEffects, RGSetupCallback setup, RGExecuteCallback execute);
        TextureHandle AllocatePhysicalTexture(const TextureDesc& desc);
        BufferHandle AllocatePhysicalBuffer(const BufferDesc& desc);
        void ReleasePhysicalTexture(TextureHandle handle, const TextureDesc& desc);
        void ReleasePhysicalBuffer(BufferHandle handle, const BufferDesc& desc);
        void ApplyBarriers(RGPass* pass, IRHICommandList* cmd);
        void ApplyFinalBarriers(uint32_t passIdx, IRHICommandList* cmd);
        void BuildDebugSnapshot();

      private:
        IRHIDevice* m_device;

        Core::SlotMap<RGTextureHandle, RGTextureResource> m_textures;
        Core::SlotMap<RGBufferHandle, RGBufferResource> m_buffers;
        Core::SlotMap<RGPassHandle, RGPass> m_passes;

        std::vector<RGPassHandle> m_passInsertionOrder;
        std::vector<RGPassHandle> m_sortedPasses;
        uint32_t m_lastExecutedPassCount = 0;
        bool m_compileSucceeded = false;
        std::vector<std::string> m_compileErrors;
        RenderGraphDebugSnapshot m_debugSnapshot;

        std::unordered_map<RGTextureHandle, ResourceState> m_textureStateMap;
        std::unordered_map<RGBufferHandle, ResourceState> m_bufferStateMap;

        struct TexturePoolEntry
        {
            TextureHandle handle;
            uint32_t lastUsedFrame;
        };

        // 物理显存池
        std::unordered_map<uint64_t, std::vector<TexturePoolEntry>> m_physicalTexturePool;
        std::unordered_map<uint64_t, std::vector<BufferHandle>> m_physicalBufferPool;
        uint64_t ComputeTextureHash(const TextureDesc& desc);
        uint64_t ComputeBufferHash(const BufferDesc& desc);
    };

} // namespace ChikaEngine::Render
