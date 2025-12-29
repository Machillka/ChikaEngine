#pragma once
#include <string>

namespace ChikaEngine::Platform
{
    // 实现跨平台窗口系统 所以归到 Platform 层
    class WindowSystem
    {
      public:
        virtual ~WindowSystem() = default;

        virtual void CreateWindow(const std::string& title, int width, int height) = 0;
        virtual void DestroyWindow() = 0;
    };
} // namespace ChikaEngine::Platform