#pragma once

#include <cstdint>
namespace ChikaEngine::Input
{
    enum class KeyCode : std::uint16_t
    {
        Unknow = 0,

        // Letter
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,

        Num0,
        Num1,
        Num2,
        Num3,
        Num4,
        Num5,
        Num6,
        Num7,
        Num8,
        Num9,

        Escape,
        Tab,
        CapsLock,
        LeftShift,
        RightShift,
        LeftCtrl,
        RightCtrl,
        LeftAlt,
        RightAlt,
        Space,
        Enter,
        Backspace,

        Up,
        Down,
        Left,
        Right,

        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
    };

    enum class MouseButton : std::uint8_t
    {
        Left = 0,
        Right = 1,
        Middle = 2,
    };
} // namespace ChikaEngine::Input