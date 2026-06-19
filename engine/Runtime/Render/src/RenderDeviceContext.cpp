#include "ChikaEngine/RenderDeviceContext.hpp"

#include <stdexcept>

namespace ChikaEngine::Render
{
    void RenderDeviceContext::Initialize(const RenderDeviceContextCreateInfo& createInfo)
    {
        Shutdown();
        const RHI_InitParams params{
            .nativeWindowHandle = createInfo.windowHandle,
            .width = createInfo.width,
            .height = createInfo.height,
            .enableValidation = createInfo.enableValidation,
            .vSync = createInfo.vSync,
        };
        m_rhi = RHIBackendFactory::CreateRHIDevice(createInfo.backendType, params);
        if (!m_rhi)
            throw std::runtime_error("Failed to create RHI device");
    }

    void RenderDeviceContext::Shutdown()
    {
        if (!m_rhi)
            return;
        m_rhi->WaitIdle();
        m_rhi.reset();
    }

    bool RenderDeviceContext::BeginFrame()
    {
        if (m_rhi)
        {
            m_rhi->BeginFrame();
            return m_rhi->IsFrameActive();
        }
        return false;
    }

    void RenderDeviceContext::EndFrame()
    {
        if (m_rhi)
            m_rhi->EndFrame();
    }

    void RenderDeviceContext::Resize(uint32_t width, uint32_t height)
    {
        if (!m_rhi || width == 0 || height == 0)
            return;
        m_rhi->WaitIdle();
        m_rhi->Resize(width, height);
    }
} // namespace ChikaEngine::Render
