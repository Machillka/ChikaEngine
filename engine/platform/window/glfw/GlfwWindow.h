#pragma once

#include "window/window_desc.h"
#include "window/window_system.h"

struct GLFWwindow; // 占位
namespace ChikaEngine::Platform
{
    class GlfwWindow final : IWindowSystem
    {
      public:
        GlfwWindow(const WindowDesc& desc);
        ~GlfwWindow() override;
        void PollEvents() override;
        void SwapBuffers() override;
        bool ShouldClose() const override;
        std::uint32_t GetWidth() const override
        {
            return _width;
        }
        std::uint32_t GetHeight() const override
        {
            return _height;
        }
        void* GetNativeHandle() const override;
        void* GetNativeContext() const override;
        void SetVSync(bool enabled) override;
        bool IsVSync() const override
        {
            return _vSync;
        }

        // glfw 特有接口
      private:
        void InitGLFW(const WindowDesc& desc);
        void InitGLAD();

      private:
        GLFWwindow* _window{nullptr};
        std::uint32_t _width{0};
        std::uint32_t _height{0};
        bool _vSync{true};
    };

} // namespace ChikaEngine::Platform