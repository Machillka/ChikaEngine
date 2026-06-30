#pragma once

#include "ChikaEngine/RenderVisibility.hpp"

#include <cstdint>

namespace ChikaEngine::Jobs
{
    class JobSystem;
}

namespace ChikaEngine::Render
{
    struct ParallelVisibilityConfig
    {
        uint32_t minimumObjects = 2'048;
        uint32_t grainSize = 256;
    };

    /** @brief Builds per-chunk visibility outputs and merges them by chunk index. */
    VisibilityResult BuildVisibilityParallel(Jobs::JobSystem& jobs, const RenderSceneView& scene, const RenderView& view, bool shadowCastersOnly, const ParallelVisibilityConfig& config = {});
} // namespace ChikaEngine::Render
