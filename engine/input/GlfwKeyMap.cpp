#include "GlfwKeyMap.h"

#include <GLFW/glfw3.h>

namespace ChikaEngine::Input
{
    const std::unordered_map<KeyCode, int>& GetKeyCodeToGlfwMap()
    {
        static const std::unordered_map<KeyCode, int> map = {
            // 字母键
            {KeyCode::A, GLFW_KEY_A},
            {KeyCode::B, GLFW_KEY_B},
            {KeyCode::C, GLFW_KEY_C},
            {KeyCode::D, GLFW_KEY_D},
            {KeyCode::E, GLFW_KEY_E},
            {KeyCode::F, GLFW_KEY_F},
            {KeyCode::G, GLFW_KEY_G},
            {KeyCode::H, GLFW_KEY_H},
            {KeyCode::I, GLFW_KEY_I},
            {KeyCode::J, GLFW_KEY_J},
            {KeyCode::K, GLFW_KEY_K},
            {KeyCode::L, GLFW_KEY_L},
            {KeyCode::M, GLFW_KEY_M},
            {KeyCode::N, GLFW_KEY_N},
            {KeyCode::O, GLFW_KEY_O},
            {KeyCode::P, GLFW_KEY_P},
            {KeyCode::Q, GLFW_KEY_Q},
            {KeyCode::R, GLFW_KEY_R},
            {KeyCode::S, GLFW_KEY_S},
            {KeyCode::T, GLFW_KEY_T},
            {KeyCode::U, GLFW_KEY_U},
            {KeyCode::V, GLFW_KEY_V},
            {KeyCode::W, GLFW_KEY_W},
            {KeyCode::X, GLFW_KEY_X},
            {KeyCode::Y, GLFW_KEY_Y},
            {KeyCode::Z, GLFW_KEY_Z},

            // 数字键（主键盘）
            {KeyCode::Num0, GLFW_KEY_0},
            {KeyCode::Num1, GLFW_KEY_1},
            {KeyCode::Num2, GLFW_KEY_2},
            {KeyCode::Num3, GLFW_KEY_3},
            {KeyCode::Num4, GLFW_KEY_4},
            {KeyCode::Num5, GLFW_KEY_5},
            {KeyCode::Num6, GLFW_KEY_6},
            {KeyCode::Num7, GLFW_KEY_7},
            {KeyCode::Num8, GLFW_KEY_8},
            {KeyCode::Num9, GLFW_KEY_9},

            // 控制键
            {KeyCode::Escape, GLFW_KEY_ESCAPE},
            {KeyCode::Tab, GLFW_KEY_TAB},
            {KeyCode::CapsLock, GLFW_KEY_CAPS_LOCK},
            {KeyCode::LeftShift, GLFW_KEY_LEFT_SHIFT},
            {KeyCode::RightShift, GLFW_KEY_RIGHT_SHIFT},
            {KeyCode::LeftCtrl, GLFW_KEY_LEFT_CONTROL},
            {KeyCode::RightCtrl, GLFW_KEY_RIGHT_CONTROL},
            {KeyCode::LeftAlt, GLFW_KEY_LEFT_ALT},
            {KeyCode::RightAlt, GLFW_KEY_RIGHT_ALT},
            {KeyCode::Space, GLFW_KEY_SPACE},
            {KeyCode::Enter, GLFW_KEY_ENTER},
            {KeyCode::Backspace, GLFW_KEY_BACKSPACE},

            // 方向键
            {KeyCode::Up, GLFW_KEY_UP},
            {KeyCode::Down, GLFW_KEY_DOWN},
            {KeyCode::Left, GLFW_KEY_LEFT},
            {KeyCode::Right, GLFW_KEY_RIGHT},

            // 功能键
            {KeyCode::F1, GLFW_KEY_F1},
            {KeyCode::F2, GLFW_KEY_F2},
            {KeyCode::F3, GLFW_KEY_F3},
            {KeyCode::F4, GLFW_KEY_F4},
            {KeyCode::F5, GLFW_KEY_F5},
            {KeyCode::F6, GLFW_KEY_F6},
            {KeyCode::F7, GLFW_KEY_F7},
            {KeyCode::F8, GLFW_KEY_F8},
            {KeyCode::F9, GLFW_KEY_F9},
            {KeyCode::F10, GLFW_KEY_F10},
            {KeyCode::F11, GLFW_KEY_F11},
            {KeyCode::F12, GLFW_KEY_F12},
        };

        return map;
    }
} // namespace ChikaEngine::Input
