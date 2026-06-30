#pragma once

#include "ChikaEngine/RenderQueue.hpp"

#include <cstdint>

namespace ChikaEngine::Jobs
{
    class JobSystem;
}

namespace ChikaEngine::Render
{
    struct ParallelPacketConfig
    {
        uint32_t minimumObjects = 2'048;
        uint32_t grainSize = 256;
    };

    /** @brief Builds main and shadow packets in chunk-local queue sets and merges by chunk index. */
    RenderQueueSet BuildRenderPacketsParallel(Jobs::JobSystem& jobs, const VisibilityResult& mainVisibility, const VisibilityResult& shadowVisibility, const RenderView& view, const RenderResourceView& resources, const ParallelPacketConfig& config = {});
} // namespace ChikaEngine::Render
