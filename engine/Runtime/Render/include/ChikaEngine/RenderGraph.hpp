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

namespace ChikaEngine::Render
{
    class IRHICommandList;

    class RenderGraph
    {
      public:
        RenderGraph(IRHIDevice* device);
        ~RenderGraph();

        RGTextureHandle ImportTexture(const std::string& name, TextureHandle handle, const TextureDesc& desc);
        void UpdateImportedTexture(RGTextureHandle handle, TextureHandle newHandle);

        void AddPass(const std::string& name, RGSetupCallback setup, RGExecuteCallback execute);
        // 单独把 资源和呈现 pass 提取, 避免手动进行 if else 判断
        void AddUploadPass(const std::string& name, RGExecuteCallback execute);
        void AddPresentPass(const std::string& name, RGTextureHandle swapchainHandle);

        void Compile();
        void Execute();

        /**
         * @brief 返回最近一次 Compile 生成的实际执行顺序。
         *
         * 该接口用于自动测试和调试工具验证 Pass 剔除与依赖调度，不暴露内部 Pass 所有权。
         */
        std::vector<std::string> GetCompiledPassNames() const;

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
        TextureHandle GetPhysicalTexture(RGTextureHandle handle);
        const TextureDesc& GetTextureDesc(RGTextureHandle handle);

      private:
        TextureHandle AllocatePhysicalTexture(const TextureDesc& desc);
        void ReleasePhysicalTexture(TextureHandle handle, const TextureDesc& desc);
        void ApplyBarriers(RGPass* pass, IRHICommandList* cmd);

      private:
        IRHIDevice* m_device;

        Core::SlotMap<RGTextureHandle, RGTextureResource> m_textures;
        Core::SlotMap<RGPassHandle, RGPass> m_passes;

        std::vector<RGPassHandle> m_passInsertionOrder;
        std::vector<RGPassHandle> m_sortedPasses;
        uint32_t m_lastExecutedPassCount = 0;

        std::unordered_map<RGTextureHandle, ResourceState> m_resourceStateMap;

        struct TexturePoolEntry
        {
            TextureHandle handle;
            uint32_t lastUsedFrame;
        };

        // 物理显存池
        std::unordered_map<uint64_t, std::vector<TexturePoolEntry>> m_physicalTexturePool;
        uint64_t ComputeTextureHash(const TextureDesc& desc);
    };

} // namespace ChikaEngine::Render
