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
        TimeBackendTypes backend;
    };
} // namespace ChikaEngine::Time