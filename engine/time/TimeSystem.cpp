#include "TimeSystem.h"

#include "GlfwTimeBackend.h"
#include "TimeBackendFactory.h"
#include "TimeDesc.h"

#include <memory>

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

    void TimeSystem::Init(TimeDesc desc)
    {
        TimeSystem::Init(TimeBackendFactory(desc));
    }
    void TimeSystem::Init(std::unique_ptr<ITimeBackend> backend)
    {
        s_backend = std::move(backend);
        s_startTime = s_backend->GetCurrentTimeSeconds();
        s_lastFrameTime = s_startTime;
    }

    void TimeSystem::Update()
    {
        double currentTime = s_backend->GetCurrentTimeSeconds();
        double rawTime = currentTime - s_lastFrameTime;
        s_lastFrameTime = currentTime;
        if (s_isPaused)
        {
            s_deltaTime = 0.0f;
            // 暂停的时候停止更新时间逻辑
            return;
        }
        s_deltaTime = static_cast<float>(rawTime) * s_timeScale;
        s_accumulatedTime += s_deltaTime;
        ++s_frameCount;
        if (s_accumulatedTime >= 1.0f)
        {
            s_fps = static_cast<float>(s_frameCount) / s_accumulatedTime;
            s_frameTime = 1000.0f / s_fps;
            s_accumulatedTime = 0.0f;
            s_frameCount = 0;
        }
    }

    float TimeSystem::GetDeltaTime()
    {
        return s_deltaTime;
    }

    float TimeSystem::GetTotalTime()
    {
        return static_cast<float>(s_backend->GetCurrentTimeSeconds()) - s_startTime;
    }

    float TimeSystem::GetTimeScale()
    {
        return s_timeScale;
    }

    float TimeSystem::GetCurrentTime()
    {
        return static_cast<float>(s_backend->GetCurrentTimeSeconds());
    }

    void TimeSystem::Pause()
    {
        s_isPaused = true;
    }

    void TimeSystem::Resume()
    {
        s_isPaused = false;
    }

    // true for pause;
    bool TimeSystem::IsPaused()
    {
        return s_isPaused;
    }

    float TimeSystem::GetFPS()
    {
        return s_fps;
    }

    float TimeSystem::GetFrameTime()
    {
        return s_frameTime;
    }
} // namespace ChikaEngine::Time