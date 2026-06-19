#include "ChikaEngine/profiler/ProfilerClock.hpp"

#include <chrono>

namespace ChikaEngine::Profiler
{
    uint64_t ProfilerClock::NowNanoseconds()
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }
} // namespace ChikaEngine::Profiler
