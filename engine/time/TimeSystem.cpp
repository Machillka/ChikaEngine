#include "TimeSystem.h"

namespace ChikaEngine::Time
{
    double TimeSystem::s_startTime = 0.0;
    double TimeSystem::s_lastFrameTime = 0.0;
    float TimeSystem::s_deltaTime = 0.0f;
    float TimeSystem::s_timeScale = 1.0f;
    bool TimeSystem::s_isPaused = false;

    float TimeSystem::s_fps = 0.0f;
    float TimeSystem::s_frameTime = 0.0f;
    float TimeSystem::s_accumulatedTime = 0.0f;
    int TimeSystem::s_frameCount = 0;

    void TimeSystem::Init() {}
} // namespace ChikaEngine::Time