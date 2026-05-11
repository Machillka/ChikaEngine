#pragma once

#include "IWindow.hpp"
#include <memory>
namespace ChikaEngine::Platform
{
    class WindowFactory
    {
      public:
        static std::unique_ptr<IWindow> Create(const WindowDesc& desc);
    };
} // namespace ChikaEngine::Platform