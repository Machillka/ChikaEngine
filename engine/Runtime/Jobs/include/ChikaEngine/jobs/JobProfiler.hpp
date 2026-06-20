#pragma once

#include "ChikaEngine/jobs/JobTypes.hpp"

#include <cstdint>

namespace ChikaEngine::Jobs
{
    /** @brief Emits scheduler events through the generic profiler without exposing job internals. */
    class JobProfiler
    {
      public:
        /** @brief Records causal lifecycle instants carrying the generation handle as payload. */
        static void Enqueued(JobHandle handle);
        static void Started(JobHandle handle);
        static void Completed(JobHandle handle);
        static void Stolen(JobHandle handle);
        static void Cancelled(JobHandle handle);
        static void Failed(JobHandle handle);

        /** @brief Records scheduler occupancy and latency counters for timeline inspection. */
        static void QueueDepth(uint32_t depth);
        static void ActiveWorkers(uint32_t count);
        static void SleepingWorkers(uint32_t count);
        static void StealCount(uint64_t count);
        static void QueueWait(uint64_t nanoseconds);
    };
} // namespace ChikaEngine::Jobs
