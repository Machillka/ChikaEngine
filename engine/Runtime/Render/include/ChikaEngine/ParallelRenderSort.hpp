#pragma once

#include "ChikaEngine/RenderQueue.hpp"

#include <cstdint>

namespace ChikaEngine::Jobs
{
    class JobSystem;
}

namespace ChikaEngine::Render
{
    struct ParallelSortConfig
    {
        uint32_t minimumPackets = 4'096;
        uint32_t grainSize = 1'024;
    };

    /** @brief Performs stable chunk sorts and deterministic serial merge for opaque packets. */
    void SortAndBuildRenderBatchesParallel(Jobs::JobSystem& jobs, RenderQueue& queue, const ParallelSortConfig& config = {});

    /** @brief Sorts opaque pass queues in jobs mode while preserving serial transparent ordering. */
    void SortAndBuildRenderQueueSetParallel(Jobs::JobSystem& jobs, RenderQueueSet& queues, const ParallelSortConfig& config = {});
} // namespace ChikaEngine::Render
