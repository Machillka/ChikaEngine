#pragma once

#include "ChikaEngine/profiler/ProfilerEvent.hpp"

#include <atomic>
#include <cstddef>
#include <vector>

namespace ChikaEngine::Profiler
{
    /**
     * @brief Fixed-capacity SPSC ring used by one instrumented thread and one aggregator.
     *
     * The producer never allocates or blocks. Overflow increments a counter and preserves every
     * already-published event instead of overwriting data the consumer has not observed.
     */
    class ProfilerEventBuffer
    {
      public:
        explicit ProfilerEventBuffer(size_t capacity = 65'536);

        bool TryPush(const ProfilerEvent& event);
        size_t Drain(std::vector<ProfilerEvent>& destination);
        uint64_t ConsumeDroppedCount();
        size_t GetCapacity() const
        {
            return m_events.size();
        }

      private:
        std::vector<ProfilerEvent> m_events;
        alignas(64) std::atomic<uint64_t> m_writeIndex{ 0 };
        alignas(64) std::atomic<uint64_t> m_readIndex{ 0 };
        std::atomic<uint64_t> m_droppedCount{ 0 };
    };
} // namespace ChikaEngine::Profiler
