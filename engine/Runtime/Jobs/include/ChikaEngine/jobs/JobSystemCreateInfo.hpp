#pragma once

#include "ChikaEngine/jobs/JobTypes.hpp"

#include <cstddef>
#include <cstdint>

namespace ChikaEngine::Jobs
{
    struct JobSystemCreateInfo
    {
        uint32_t workerCount = 0;
        uint32_t reservedThreads = 2;
        size_t jobCapacity = 65'536;
        size_t injectionQueueCapacity = 65'536;
        size_t localQueueCapacity = 4'096;
        uint32_t stealSeed = 0xC11CAu;
        uint32_t parallelForMinItems = 256;
        uint32_t maximumParallelChunks = 1'024;
        JobShutdownPolicy shutdownPolicy = JobShutdownPolicy::Drain;
        uint32_t failWorkerStartAt = UINT32_MAX;
    };
} // namespace ChikaEngine::Jobs
