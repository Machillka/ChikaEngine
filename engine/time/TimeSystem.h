#pragma once

#include "ITimeBackend.h"
#include "TimeDesc.h"

#include <memory>
namespace ChikaEngine::Time
{
    class TimeSystem
    {
      public:
        static void Init(std::unique_ptr<ITimeBackend> backend);
        static void Init(TimeDesc desc);
        static void Update(); // 每帧更新 并且计算 fps

        static float GetDeltaTime(); // deltaTime
        static float GetTotalTime(); // Totla Time
        static float GetTimeScale();
        static void SetTimeScale(float scale);
        static float GetCurrentTime(); // 得到当前时间
        static void Pause();
        static void Resume();
        static bool IsPaused();

        static float GetFPS();
        static float GetFrameTime();

      private:
        // NOTE: inline ?? -> 初始化问题
        inline static std::unique_ptr<ITimeBackend> s_backend{};

        static double s_startTime;
        static double s_lastFrameTime;
        static float s_deltaTime;
        static float s_timeScale;
        static bool s_isPaused;

        static float s_fps;
        static float s_frameTime;
        static float s_accumulatedTime;
        static int s_frameCount;
    };
} // namespace ChikaEngine::Time