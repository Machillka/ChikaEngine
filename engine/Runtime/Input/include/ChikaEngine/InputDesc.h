#pragma once

namespace ChikaEngine::Input
{
    enum class InputBackendTypes
    {
        None,
        GLFW,
    };
    struct InputDesc
    {
        InputBackendTypes backendType;
    };
} // namespace ChikaEngine::Input