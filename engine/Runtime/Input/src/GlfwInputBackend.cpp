
#include "ChikaEngine/GlfwInputBackend.h"
#include "ChikaEngine/GlfwKeyMap.h"
#include "GLFW/glfw3.h"

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
            _currentMouse[i] = glfwGetMouseButton(_window, i) == GLFW_PRESS;
        }

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
        if (!IsValidMouseButtonIndex(idx))
            return false;
        return _currentMouse[idx] && !_prevMouse[idx];
    }

    bool GlfwInputBackend::GetMouseButtonUp(MouseButton button) const
    {
        int idx = ToIndex(button);
        if (!IsValidMouseButtonIndex(idx))
            return false;
        return !_currentMouse[idx] && _prevMouse[idx];
    }

    bool GlfwInputBackend::GetMouseButton(MouseButton button) const
    {
        const int idx = ToIndex(button);
        return IsValidMouseButtonIndex(idx) && _currentMouse[idx];
    }

    std::pair<double, double> GlfwInputBackend::GetMousePosition() const
    {
        return { _mouseX, _mouseY };
    }

    std::pair<double, double> GlfwInputBackend::GetMouseDelta() const
    {
        return { _mouseX - _prevMouseX, _mouseY - _prevMouseY };
    }

    int GlfwInputBackend::ToIndex(KeyCode key) const
    {
        return static_cast<int>(key);
    }

    int GlfwInputBackend::ToIndex(MouseButton button) const
    {
        return static_cast<int>(button);
    }

    bool GlfwInputBackend::IsValidMouseButtonIndex(int index) const
    {
        return index >= 0 && index < MaxMouseButtonCounts;
    }
} // namespace ChikaEngine::Input
