#pragma once

namespace ChikaEngine::Time
{


    // Time Provider Backend Interface
    // 只需要借用得到时间的function即可，于是具体的计算逻辑直接放在Time System 实现
    class ITimeBackend
    {
      public:
        virtual ~ITimeBackend() = default;
        virtual double GetCurrentTimeSeconds() const = 0;
    };
} // namespace ChikaEngine::Time