#pragma once
#include <cstdint>

namespace ChikaEngine::Platform
{
    // 实现跨平台窗口系统抽象接口
    class IWindow
    {
      public:
        virtual ~IWindow() = default;

        virtual std::uint32_t GetWidth() const = 0;
        virtual std::uint32_t GetHeight() const = 0;

        virtual void PollEvents() = 0;
        virtual void SwapBuffers() = 0;
        virtual bool ShouldClose() const = 0;

        virtual void* GetNativeHandle() const = 0;
        virtual void* GetNativeContext() const = 0;

        virtual void SetVSync(bool enabled) = 0;
        virtual bool IsVSync() const = 0;
    };

} // namespace ChikaEngine::Platform