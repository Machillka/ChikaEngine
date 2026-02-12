#include "ChikaEngine/window/window_desc.h"
#include "ChikaEngine/window/GLFW/GlfwWindow.h"

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace ChikaEngine::Platform
{
    GlfwWindow::GlfwWindow(const WindowDesc& desc)
    {
        InitGLFW(desc);
        InitGLAD();
        SetVSync(desc.vSync);
    }

    GlfwWindow::~GlfwWindow()
    {
        if (_window)
        {
            glfwDestroyWindow(_window);
            _window = nullptr;
        }
        glfwTerminate();
    }

    void GlfwWindow::InitGLFW(const WindowDesc& desc)
    {
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        _window = glfwCreateWindow(static_cast<int>(desc.width), static_cast<int>(desc.height), desc.title.c_str(), nullptr, nullptr);
        if (!_window)
            throw std::runtime_error("Failed to create GLFW window");
        glfwMakeContextCurrent(_window);
        _width = desc.width;
        _height = desc.height;
        glfwSetWindowUserPointer(_window, this);
        glfwSetFramebufferSizeCallback(_window,
                                       [](GLFWwindow* win, int w, int h)
                                       {
                                           auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
                                           self->_width = static_cast<std::uint32_t>(w);
                                           self->_height = static_cast<std::uint32_t>(h);
                                           glViewport(0, 0, w, h);
                                       });
    }

    void GlfwWindow::InitGLAD()
    {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            throw std::runtime_error("Failed to initialize GLAD");
    }

    void GlfwWindow::PollEvents()
    {
        glfwPollEvents();
    }

    void GlfwWindow::SwapBuffers()
    {
        glfwSwapBuffers(_window);
    }

    bool GlfwWindow::ShouldClose() const
    {
        return glfwWindowShouldClose(_window);
    }

    void* GlfwWindow::GetNativeHandle() const
    {
        return _window;
    }

    void* GlfwWindow::GetNativeContext() const
    {
        // GLFW 隐式管理 OpenGL context
        return nullptr;
    }

    void GlfwWindow::SetVSync(const bool enabled)
    {
        _vSync = enabled;
        glfwSwapInterval(_vSync ? 1 : 0);
    }
} // namespace ChikaEngine::Platform