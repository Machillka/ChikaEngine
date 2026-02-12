#pragma once
#include "window_desc.h"
#include "window_system.h"

#include <memory>

namespace ChikaEngine::Platform
{
    // 窗口工厂函数
    std::unique_ptr<IWindow> CreateWindow(const WindowDesc& desc, WindowBackend backendType);
} // namespace ChikaEngine::Platform