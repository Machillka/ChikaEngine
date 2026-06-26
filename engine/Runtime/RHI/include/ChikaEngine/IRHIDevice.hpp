#pragma once

#include "IRHICommandList.hpp"
#include "RenderDiagnostics.hpp"
#include "RHICapabilities.hpp"
#include "RHIDesc.hpp"
#include "RHIResourceHandle.hpp"
#include <cstdint>
#include <filesystem>
#include <string_view>
namespace ChikaEngine::Render
{

    struct RHI_InitParams
    {
        void* nativeWindowHandle = nullptr;
        uint32_t width = 1280;
        uint32_t height = 720;
        bool enableValidation = true;
        bool vSync = true;
        std::filesystem::path pipelineCachePath = ".chika/cache/vulkan_pipeline_cache.bin";
    };

    class IRHIDevice
    {
      public:
        virtual ~IRHIDevice() = default;

        // 初始化与销毁
        virtual void Initialize(const RHI_InitParams& params) = 0;
        virtual void Shutdown() = 0;

        // 每帧生命周期
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        /** @brief Reports whether BeginFrame acquired a usable target for command recording. */
        virtual bool IsFrameActive() const
        {
            return true;
        }
        /** @brief Returns the selected adapter name for diagnostics and benchmark metadata. */
        virtual std::string_view GetDeviceName() const
        {
            return "Unknown RHI device";
        }
        /**
         * @brief Returns the backend-independent feature set enabled on the selected RHI device.
         *
         * The default is intentionally conservative so mock and legacy devices fall back to CPU paths
         * until they explicitly opt into GPU-driven features.
         */
        virtual const RHICapabilities& GetCapabilities() const
        {
            static const RHICapabilities capabilities{};
            return capabilities;
        }

        // 资源创建（返回 RHI 句柄）
        virtual BufferHandle CreateBuffer(const BufferDesc& desc) = 0;
        virtual TextureHandle CreateTexture(const TextureDesc& desc) = 0;
        virtual SamplerHandle CreateSampler(const SamplerDesc& desc) = 0;
        virtual TextureViewHandle CreateTextureView(const TextureViewDesc& desc) = 0;
        virtual ShaderHandle CreateShader(const ShaderDesc& desc) = 0;
        virtual PipelineHandle CreateGraphicsPipeline(const PipelineDesc& desc) = 0;
        virtual PipelineHandle CreateComputePipeline(const ComputePipelineDesc& desc) = 0;

        virtual void* GetMappedData(BufferHandle handle) = 0;

        /**
         * @brief 为 RHI 资源设置调试名称，供 Validation Layer、RenderDoc 和 GPU 调试器显示。
         *
         * 名称不参与资源身份或生命周期，只用于提高诊断信息可读性。
         */
        virtual void SetDebugName(BufferHandle handle, std::string_view name) = 0;
        virtual void SetDebugName(TextureHandle handle, std::string_view name) = 0;
        virtual void SetDebugName(ShaderHandle handle, std::string_view name) = 0;
        virtual void SetDebugName(PipelineHandle handle, std::string_view name) = 0;
        virtual void SetDebugName(SamplerHandle handle, std::string_view name) = 0;
        virtual void SetDebugName(TextureViewHandle handle, std::string_view name) = 0;

        /**
         * @brief 返回当前帧由 RHI 收集的命令统计。
         *
         * Renderer 会在 RenderGraph 执行后补充 Pass 数量，形成最终帧统计。
         */
        virtual const RenderFrameStatistics& GetFrameStatistics() const = 0;
        virtual const std::vector<RenderPassGpuTiming>& GetPassGpuTimings() const = 0;

        // Create -> Allocate
        virtual IRHICommandList* AllocateCommandList() = 0;
        virtual void Submit(IRHICommandList* cmdList) = 0;

        virtual void DestroyBuffer(BufferHandle handle) = 0;
        virtual void DestroyTexture(TextureHandle handle) = 0;
        virtual void DestroyShader(ShaderHandle handle) = 0;
        virtual void DestroyPipeline(PipelineHandle handle) = 0;
        virtual void DestroySampler(SamplerHandle handle) = 0;
        virtual void DestroyTextureView(TextureViewHandle handle) = 0;

        virtual TextureHandle GetActiveSwapchainTexture() = 0;

        // CPU 等待 GPU 执行完所有绘制操作
        virtual void WaitIdle() = 0;

        // 用于重建 swapchain 等
        virtual void Resize(uint32_t width, uint32_t height) = 0;
    };
} // namespace ChikaEngine::Render
