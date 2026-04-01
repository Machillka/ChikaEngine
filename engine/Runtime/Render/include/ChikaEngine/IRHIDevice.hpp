#pragma once

#include "IRHICommandList.hpp"
#include "RHIDesc.hpp"
#include "RHIResourceHandle.hpp"
#include <cstdint>
namespace ChikaEngine::Render
{

    struct RHI_InitParams
    {
        void* nativeWindowHandle = nullptr;
        uint32_t width = 1280;
        uint32_t height = 720;
        bool enableValidation = true;
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

        // 资源创建（返回 RHI 句柄）
        virtual BufferHandle CreateBuffer(const BufferDesc& desc) = 0;
        virtual TextureHandle CreateTexture(const TextureDesc& desc) = 0;
        virtual ShaderHandle CreateShader(const ShaderDesc& desc) = 0;
        virtual PipelineHandle CreateGraphicsPipeline(const PipelineDesc& desc) = 0;

        virtual void* GetMappedData(BufferHandle handle) = 0;

        // Create -> Allocate
        virtual IRHICommandList* AllocateCommandList() = 0;
        virtual void Submit(IRHICommandList* cmdList) = 0;

        virtual void DestroyBuffer(BufferHandle handle) = 0;
        virtual void DestroyTexture(TextureHandle handle) = 0;

        virtual TextureHandle GetActiveSwapchainTexture() = 0;

        // 暴露 texture handle 给上层 如 imgui 等 display
        // virtual void GetTextureHandle(TextureHandle handle) = 0;
        // 对 imgui 特化
        virtual void* GetImGuiTextureHandle(TextureHandle handle) = 0;
    };
} // namespace ChikaEngine::Render