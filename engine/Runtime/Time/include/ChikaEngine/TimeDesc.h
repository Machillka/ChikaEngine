#pragma once

namespace ChikaEngine::Time
{

    // 后端类型
    enum class TimeBackendTypes
    {
        None,
        GLFW,
    };

    struct TimeDesc
    {
        TimeBackendTypes backend = TimeBackendTypes::GLFW;
    };
} // namespace ChikaEngine::Time
