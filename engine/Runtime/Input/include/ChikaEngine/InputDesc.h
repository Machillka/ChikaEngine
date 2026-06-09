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
        InputBackendTypes backendType = InputBackendTypes::GLFW;
    };
} // namespace ChikaEngine::Input
