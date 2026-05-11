#pragma once
#include "ChikaEngine/Window/IWindow.hpp"

struct GLFWwindow;

namespace ChikaEngine::Platform
{
    class GlfwWindow : public IWindow
    {
      public:
        GlfwWindow() = default;
        ~GlfwWindow() override;

        void Initialize(const WindowDesc& desc) override;
        void Shutdown() override;
        void Tick() override;

        bool ShouldClose() const override;

        uint32_t GetWidth() const override
        {
            return m_data.width;
        }

        uint32_t GetHeight() const override
        {
            return m_data.height;
        }

        void* GetNativeHandle() const override
        {
            return m_window;
        }

        void SetFullScreen(bool fullscreen) override;

        bool IsFullScreen() const override
        {
            return m_data.isFullscreen;
        }

        void SetResizeCallback(const WindowResizeCallback& callback) override
        {
            m_data.resizeCallback = callback;
        }

      private:
        GLFWwindow* m_window = nullptr;

        // 将所有状态封装到一个 Data 结构体中，方便传给 GLFW 的 C 回调函数
        struct WindowData
        {
            std::string title;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            bool isFullscreen = false;

            // 用于从全屏恢复窗口时的数据备份
            int windowedX = 0, windowedY = 0;
            int windowedWidth = 1280, windowedHeight = 720;

            WindowResizeCallback resizeCallback;
        };

        WindowData m_data;
    };
} // namespace ChikaEngine::Platform