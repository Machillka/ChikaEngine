#include "GlfwInputBackend.h"

#include "GLFW/glfw3.h"
#include "GlfwKeyMap.h"

namespace ChikaEngine::Input
{
    GlfwInputBackend::GlfwInputBackend(GLFWwindow* window) : _window(window) {}
    GlfwInputBackend::GlfwInputBackend(void* window) : _window(static_cast<GLFWwindow*>(window)) {}
    void GlfwInputBackend::Update()
    {
        _prevKeys = _currentKeys;
        _prevMouse = _currentMouse;
        _prevMouseX = _mouseX;
        _prevMouseY = _mouseY;

        const auto& map = GetKeyCodeToGlfwMap();
        // 更新在 map 中注册过的键盘状态
        for (const auto& [keyCode, glfwKey] : map)
        {
            if (glfwKey < 0)
                continue;
            int idx = ToIndex(keyCode);
            _currentKeys[idx] = (glfwGetKey(_window, glfwKey) == GLFW_PRESS);
        }
        for (int i = 0; i < MaxMouseButtonCounts; ++i)
        {
            _currentMouse[i] = (glfwGetMouseButton(_window, ToGlfwMouse(static_cast<MouseButton>(i))) == GLFW_PRESS);
        }

        _currentMouse[0] = glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        _currentMouse[1] = glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        _currentMouse[2] = glfwGetMouseButton(_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        glfwGetCursorPos(_window, &_mouseX, &_mouseY);
    }

    bool GlfwInputBackend::GetKeyDown(KeyCode key) const
    {
        int idx = ToIndex(key);
        // 当前帧在 上一帧不在
        return _currentKeys[idx] && !_prevKeys[idx];
    }

    bool GlfwInputBackend::GetKeyUp(KeyCode key) const
    {
        int idx = ToIndex(key);
        // 当前不在 上一帧在
        return !_currentKeys[idx] && _prevKeys[idx];
    }

    bool GlfwInputBackend::GetKey(KeyCode key) const
    {
        return _currentKeys[ToIndex(key)];
    }

    bool GlfwInputBackend::GetMouseButtonDown(MouseButton button) const
    {
        int idx = ToIndex(button);
        return _currentMouse[idx] && !_prevMouse[idx];
    }

    bool GlfwInputBackend::GetMouseButtonUp(MouseButton button) const
    {
        int idx = ToIndex(button);
        return !_currentMouse[idx] && _prevMouse[idx];
    }

    bool GlfwInputBackend::GetMouseButton(MouseButton button) const
    {
        return _currentMouse[ToIndex(button)];
    }

    std::pair<double, double> GlfwInputBackend::GetMousePosition() const
    {
        return {_mouseX, _mouseY};
    }

    std::pair<double, double> GlfwInputBackend::GetMouseDelta() const
    {
        return {_mouseX - _prevMouseX, _mouseY - _prevMouseY};
    }

    int GlfwInputBackend::ToIndex(KeyCode key) const
    {
        return static_cast<int>(key);
    }

    int GlfwInputBackend::ToIndex(MouseButton button) const
    {
        return static_cast<int>(button);
    }

    int GlfwInputBackend::ToGlfwMouse(MouseButton button) const
    {
        switch (button)
        {
        case MouseButton::Left:
            return GLFW_MOUSE_BUTTON_LEFT;
        case MouseButton::Right:
            return GLFW_MOUSE_BUTTON_RIGHT;
        case MouseButton::Middle:
            return GLFW_MOUSE_BUTTON_MIDDLE;
        default:
            return -1;
        }
    }
} // namespace ChikaEngine::Input