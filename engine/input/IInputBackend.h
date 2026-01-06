#pragma once

#include "InputCodes.h"

#include <utility>

namespace ChikaEngine::Input
{
    class IInputBackend
    {
      public:
        virtual ~IInputBackend() = default;
        virtual void Update() = 0;

        // 监听键盘事件
        virtual bool GetKeyDown(KeyCode key) const = 0;
        virtual bool GetKeyUp(KeyCode key) const = 0;
        virtual bool GetKey(KeyCode key) const = 0;

        // 鼠标点击事件
        virtual bool GetMouseButtonDown(MouseButton button) const = 0;
        virtual bool GetMouseButtonUp(MouseButton button) const = 0;
        virtual bool GetMouseButton(MouseButton buttonb) const = 0;

        virtual std::pair<double, double> GetMousePosition() const = 0;
        virtual std::pair<double, double> GetMouseDelta() const = 0;
    };
} // namespace ChikaEngine::Input