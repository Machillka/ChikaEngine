#pragma once

#include "IInputBackend.h"
#include "InputCodes.h"

#include <GLFW/glfw3.h>
#include <array>

namespace ChikaEngine::Input
{
    class GlfwInputBackend : public IInputBackend
    {
      public:
        explicit GlfwInputBackend(GLFWwindow* window);
        explicit GlfwInputBackend(void* window);
        void Update() override;

        bool GetKeyDown(KeyCode key) const override;
        bool GetKeyUp(KeyCode key) const override;
        bool GetKey(KeyCode key) const override;

        bool GetMouseButtonDown(MouseButton button) const override;
        bool GetMouseButtonUp(MouseButton button) const override;
        bool GetMouseButton(MouseButton button) const override;

        std::pair<double, double> GetMousePosition() const override;
        std::pair<double, double> GetMouseDelta() const override;

      private:
        GLFWwindow* _window = nullptr;

        static constexpr int MaxKeyCounts = 256;
        static constexpr int MaxMouseButtonCounts = 8;

        // 缓存状态
        std::array<bool, MaxKeyCounts> _currentKeys{};
        std::array<bool, MaxKeyCounts> _prevKeys{};
        std::array<bool, MaxMouseButtonCounts> _currentMouse{};
        std::array<bool, MaxMouseButtonCounts> _prevMouse{};

        double _mouseX = 0.0;
        double _mouseY = 0.0;
        double _prevMouseX = 0.0;
        double _prevMouseY = 0.0;

      private:
        // 把枚举变量转化成 GLFW 可识别
        int ToIndex(KeyCode key) const;
        int ToIndex(MouseButton button) const;
        int ToGlfwKey(KeyCode key) const;
        int ToGlfwMouse(MouseButton button) const;
    };
} // namespace ChikaEngine::Input