#pragma once
#include "platform/window_system.h"
#include "render/render_device.h"
namespace ChikaEngine::Render
{

    class GLDevice : public IRenderDevice
    {
      public:
        explicit GLDevice(::ChikaEngine::Platform::WindowSystem& window) noexcept;
        void Init();
        void Shutdown();
        void BeginFrame();
        void EndFrame();
        void DrawObject(const RenderObject& obj);
        void OnResize(std::uint32_t width, std::uint32_t height);
    };
} // namespace ChikaEngine::Render