#include "window_factory.h"

#include "window/glfw/GlfwWindow.h"

#include <stdexcept>


namespace ChikaEngine::Platform
{
    std::unique_ptr<IWindow> CreateWindow(const WindowDesc& desc, WindowBackend backendType)
    {
        switch (backendType)
        {
        case WindowBackend::GLFW:
            return std::make_unique<GlfwWindow>(desc);
        default:
            throw std::runtime_error{"Unsupported window backend"};
        }
    }
} // namespace ChikaEngine::Platform