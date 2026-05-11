#pragma once

#include "WinDesc.hpp"
#include <cstdint>
#include <functional>
namespace ChikaEngine::Platform
{

    class IWindow
    {
      public:
        virtual ~IWindow() = default;

        // 生命周期
        virtual void Initialize(const WindowDesc& desc) = 0;
        virtual void Shutdown() = 0;
        virtual void Tick() = 0; // 处理操作系统消息分发

        // 状态查询
        virtual bool ShouldClose() const = 0;
        virtual std::uint32_t GetWidth() const = 0;
        virtual std::uint32_t GetHeight() const = 0;
        virtual void* GetNativeHandle() const = 0; // 供 Vulkan / ImGui 使用

        // 全屏控制
        virtual void SetFullScreen(bool fullscreen) = 0;
        virtual bool IsFullScreen() const = 0;

        // --- 事件回调系统 ---
        // 宽度, 高度
        using WindowResizeCallback = std::function<void(uint32_t, uint32_t)>;
        virtual void SetResizeCallback(const WindowResizeCallback& callback) = 0;
    };
} // namespace ChikaEngine::Platform