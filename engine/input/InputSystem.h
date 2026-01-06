#pragma once

#include "InputCodes.h"

#include <memory>

namespace ChikaEngine::Input
{
    class IInputBackend;
    // TODO: 加入回调
    class InputSystem
    {
      public:
        // 传递后端实现
        static void Init(std::unique_ptr<IInputBackend> backend);
        static void Shutdown();
        static void Update();
        static bool GetKeyDown(KeyCode key);
        static bool GetKeyUp(KeyCode key);
        static bool GetKey(KeyCode key);
        static bool GetMouseButtonbDown(MouseButton button);
        static bool GetMouseButtonUp(MouseButton button);
        static bool GetMouseButton(MouseButton button);

        static std::pair<double, double> GetMousePosition();
        static std::pair<double, double> GetMouseDelta();

      private:
        static std::unique_ptr<IInputBackend> s_backend;
    };
} // namespace ChikaEngine::Input