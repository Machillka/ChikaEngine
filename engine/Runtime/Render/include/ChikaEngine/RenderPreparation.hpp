#pragma once

#include "ChikaEngine/ParallelRenderQueue.hpp"
#include "ChikaEngine/ParallelRenderSort.hpp"
#include "ChikaEngine/ParallelRenderVisibility.hpp"
#include "ChikaEngine/RenderSettings.hpp"
#include "ChikaEngine/RenderValidation.hpp"

#include <memory>

namespace ChikaEngine::Jobs
{
    class JobSystem;
}

namespace ChikaEngine::Render
{
    enum class RenderPreparationFallback : uint8_t
    {
        None,
        SerialRequested,
        JobsUnavailable,
        BelowThreshold,
        ParallelFailure,
    };

    struct RenderPreparationConfig
    {
        RenderCpuMode mode = RenderCpuMode::Jobs;
        uint32_t minimumObjects = 2'048;
        uint32_t visibilityGrainSize = 256;
        uint32_t packetGrainSize = 256;
        uint32_t sortMinimumPackets = 4'096;
        uint32_t sortGrainSize = 1'024;
    };

    struct RenderPreparationDiagnostics
    {
        double totalCpuTimeMs = 0.0;
        double sceneViewCpuTimeMs = 0.0;
        double resourceViewCpuTimeMs = 0.0;
        double visibilityCpuTimeMs = 0.0;
        double packetCpuTimeMs = 0.0;
        double sortCpuTimeMs = 0.0;
        bool jobsUsed = false;
        RenderPreparationFallback fallback = RenderPreparationFallback::None;
        RenderSceneClassification classification;
        RenderValidationHashes hashes;
    };

    struct RenderPreparationResult
    {
        RenderSceneView sceneView;
        VisibilityResult mainVisibility;
        VisibilityResult shadowVisibility;
        RenderQueueSet queues;
        RenderPreparationDiagnostics diagnostics;
    };

    /** @brief Runs the serial oracle or the complete jobs path with all-stage serial fallback. */
    RenderPreparationResult PrepareRenderData(std::shared_ptr<const RenderWorldSnapshot> snapshot, const RenderView& mainView, const RenderView& shadowView, const RenderResourceView& resources, Jobs::JobSystem* jobs, const RenderPreparationConfig& config = {});
} // namespace ChikaEngine::Render
