#pragma once

#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/rhi/RHIBackendFactory.hpp"
#include <cstdint>
#include <memory>

namespace ChikaEngine::Render
{
    struct RenderDeviceContextCreateInfo
    {
        void* windowHandle = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        RHIBackendTypes backendType = RHIBackendTypes::Default;
        bool enableValidation = true;
        bool vSync = true;
    };

    /**
     * @brief 统一管理 RHI 设备与帧生命周期。
     *
     * Renderer Facade 不再直接创建、销毁或 Resize 图形后端。
     */
    class RenderDeviceContext
    {
      public:
        /** @brief 创建指定后端的 RHI 设备，使设备所有权集中在单一边界。 */
        void Initialize(const RenderDeviceContextCreateInfo& createInfo);
        /** @brief 等待 GPU 空闲后销毁 RHI，防止资源仍被提交命令引用。 */
        void Shutdown();
        /** @brief Starts an RHI frame and reports whether a swapchain image is available. */
        bool BeginFrame();
        void EndFrame();
        void Resize(uint32_t width, uint32_t height);

        IRHIDevice* GetRHI() const
        {
            return m_rhi.get();
        }

      private:
        std::unique_ptr<IRHIDevice> m_rhi;
    };
} // namespace ChikaEngine::Render
