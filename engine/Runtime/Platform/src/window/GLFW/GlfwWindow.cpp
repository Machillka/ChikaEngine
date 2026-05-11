#include "ChikaEngine/Window/GLFW/GlfwWindow.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include <GLFW/glfw3.h>

namespace ChikaEngine::Platform
{
    static void GLFWErrorCallback(int error, const char* description)
    {
        LOG_ERROR("GLFW", "GLFW Error ({0}): {1}", error, description);
    }

    GlfwWindow::~GlfwWindow()
    {
        Shutdown();
    }

    void GlfwWindow::Initialize(const WindowDesc& desc)
    {
        m_data.title = desc.title;
        m_data.width = desc.width;
        m_data.height = desc.height;
        m_data.isFullscreen = desc.isFullscreen;

        m_data.windowedWidth = desc.width;
        m_data.windowedHeight = desc.height;

        int success = glfwInit();
        if (!success)
        {
            LOG_ERROR("GLFW", "Could not initialize GLFW!");
            return;
        }

        glfwSetErrorCallback(GLFWErrorCallback);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_window = glfwCreateWindow((int)desc.width, (int)desc.height, m_data.title.c_str(), nullptr, nullptr);

        glfwSetWindowUserPointer(m_window, &m_data);

        glfwSetFramebufferSizeCallback(m_window,
                                       [](GLFWwindow* window, int width, int height)
                                       {
                                           auto& data = *(WindowData*)glfwGetWindowUserPointer(window);
                                           data.width = width;
                                           data.height = height;

                                           if (data.resizeCallback && width > 0 && height > 0)
                                           {
                                               data.resizeCallback(width, height);
                                           }
                                       });

        if (desc.isFullscreen)
        {
            SetFullScreen(true);
        }
    }

    void GlfwWindow::Shutdown()
    {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
            glfwTerminate();
        }
    }

    void GlfwWindow::Tick()
    {
        glfwPollEvents();
    }

    bool GlfwWindow::ShouldClose() const
    {
        return glfwWindowShouldClose(m_window);
    }

    void GlfwWindow::SetFullScreen(bool fullscreen)
    {
        if (m_data.isFullscreen == fullscreen)
            return;

        if (fullscreen)
        {
            glfwGetWindowPos(m_window, &m_data.windowedX, &m_data.windowedY);
            glfwGetWindowSize(m_window, &m_data.windowedWidth, &m_data.windowedHeight);

            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);

            glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else
        {
            glfwSetWindowMonitor(m_window, nullptr, m_data.windowedX, m_data.windowedY, m_data.windowedWidth, m_data.windowedHeight, 0);
        }

        m_data.isFullscreen = fullscreen;
    }
} // namespace ChikaEngine::Platform