#include "ChikaEngine/window/GLFW/GlfwWindow.h"
#include "ChikaEngine/window/window_desc.h"
#include "ChikaEngine/window/window_system.h"
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