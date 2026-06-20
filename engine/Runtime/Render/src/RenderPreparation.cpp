#include "ChikaEngine/RenderPreparation.hpp"

#include "ChikaEngine/jobs/JobSystem.hpp"
#include "ChikaEngine/profiler/ProfilerClock.hpp"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"

namespace ChikaEngine::Render
{
    namespace
    {
        double ElapsedMilliseconds(uint64_t begin)
        {
            return static_cast<double>(Profiler::ProfilerClock::NowNanoseconds() - begin) / 1'000'000.0;
        }

        /** @brief Rebuilds every derived output so a partial jobs failure cannot leak into rendering. */
        void RunSerial(RenderPreparationResult& result, const RenderView& mainView, const RenderView& shadowView, const RenderResourceView& resources)
        {
            uint64_t begin = Profiler::ProfilerClock::NowNanoseconds();
            result.mainVisibility = BuildVisibilitySerial(result.sceneView, mainView, false);
            result.shadowVisibility = BuildVisibilitySerial(result.sceneView, shadowView, true);
            result.diagnostics.visibilityCpuTimeMs = ElapsedMilliseconds(begin);

            begin = Profiler::ProfilerClock::NowNanoseconds();
            result.queues = BuildRenderPacketsSerial(result.mainVisibility, result.shadowVisibility, mainView, resources);
            result.diagnostics.packetCpuTimeMs = ElapsedMilliseconds(begin);

            begin = Profiler::ProfilerClock::NowNanoseconds();
            SortAndBuildRenderQueueSetSerial(result.queues);
            result.diagnostics.sortCpuTimeMs = ElapsedMilliseconds(begin);
        }

        /** @brief Runs object-level jobs while all resource metadata remains immutable. */
        void RunParallel(RenderPreparationResult& result, const RenderView& mainView, const RenderView& shadowView, const RenderResourceView& resources, Jobs::JobSystem& jobs, const RenderPreparationConfig& config)
        {
            uint64_t begin = Profiler::ProfilerClock::NowNanoseconds();
            const ParallelVisibilityConfig visibilityConfig{ .minimumObjects = 0, .grainSize = config.visibilityGrainSize };
            result.mainVisibility = BuildVisibilityParallel(jobs, result.sceneView, mainView, false, visibilityConfig);
            result.shadowVisibility = BuildVisibilityParallel(jobs, result.sceneView, shadowView, true, visibilityConfig);
            result.diagnostics.visibilityCpuTimeMs = ElapsedMilliseconds(begin);

            begin = Profiler::ProfilerClock::NowNanoseconds();
            result.queues = BuildRenderPacketsParallel(jobs, result.mainVisibility, result.shadowVisibility, mainView, resources, { .minimumObjects = 0, .grainSize = config.packetGrainSize });
            result.diagnostics.packetCpuTimeMs = ElapsedMilliseconds(begin);

            begin = Profiler::ProfilerClock::NowNanoseconds();
            SortAndBuildRenderQueueSetParallel(jobs, result.queues, { .minimumPackets = config.sortMinimumPackets, .grainSize = config.sortGrainSize });
            result.diagnostics.sortCpuTimeMs = ElapsedMilliseconds(begin);
        }
    } // namespace

    RenderPreparationResult PrepareRenderData(std::shared_ptr<const RenderWorldSnapshot> snapshot, const RenderView& mainView, const RenderView& shadowView, const RenderResourceView& resources, Jobs::JobSystem* jobs, const RenderPreparationConfig& config)
    {
        CHIKA_PROFILE_SCOPE("Renderer.PrepareData");
        const uint64_t totalBegin = Profiler::ProfilerClock::NowNanoseconds();
        RenderPreparationResult result;

        uint64_t begin = Profiler::ProfilerClock::NowNanoseconds();
        result.sceneView = RenderSceneView::Build(std::move(snapshot));
        result.diagnostics.sceneViewCpuTimeMs = ElapsedMilliseconds(begin);
        result.diagnostics.classification = result.sceneView.GetClassification();

        if (config.mode == RenderCpuMode::Serial)
            result.diagnostics.fallback = RenderPreparationFallback::SerialRequested;
        else if (!jobs)
            result.diagnostics.fallback = RenderPreparationFallback::JobsUnavailable;
        else if (result.sceneView.GetInstances().size() < config.minimumObjects)
            result.diagnostics.fallback = RenderPreparationFallback::BelowThreshold;
        else
        {
            try
            {
                RunParallel(result, mainView, shadowView, resources, *jobs, config);
                result.diagnostics.jobsUsed = true;
            }
            catch (...)
            {
                result.diagnostics.jobsUsed = false;
                result.diagnostics.fallback = RenderPreparationFallback::ParallelFailure;
                RunSerial(result, mainView, shadowView, resources);
            }
        }

        if (!result.diagnostics.jobsUsed && result.diagnostics.fallback != RenderPreparationFallback::ParallelFailure)
            RunSerial(result, mainView, shadowView, resources);

        result.diagnostics.hashes = BuildRenderValidationHashes(result.mainVisibility, result.queues);
        result.diagnostics.totalCpuTimeMs = ElapsedMilliseconds(totalBegin);
        return result;
    }
} // namespace ChikaEngine::Render
