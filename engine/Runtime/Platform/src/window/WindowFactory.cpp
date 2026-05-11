#include "ChikaEngine/Window/WindowFactory.hpp"
#include "ChikaEngine/Window/GLFW/GlfwWindow.hpp"

namespace ChikaEngine::Platform
{
    // TODO: 加上判断返回不同窗口类型
    std::unique_ptr<IWindow> WindowFactory::Create(const WindowDesc& desc)
    {
        auto window = std::make_unique<GlfwWindow>();
        window->Initialize(desc);
        return window;
    }
} // namespace ChikaEngine::Platform