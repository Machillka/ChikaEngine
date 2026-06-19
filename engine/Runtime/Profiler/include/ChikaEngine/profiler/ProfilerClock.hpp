#pragma once

#include <cstdint>

namespace ChikaEngine::Profiler
{
    class ProfilerClock
    {
      public:
        /** @brief Returns nanoseconds from a process-local monotonic clock. */
        static uint64_t NowNanoseconds();
    };
} // namespace ChikaEngine::Profiler
