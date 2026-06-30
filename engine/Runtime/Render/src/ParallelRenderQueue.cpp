#include "ChikaEngine/ParallelRenderQueue.hpp"

#include "ChikaEngine/jobs/ParallelFor.hpp"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"

#include <algorithm>

namespace ChikaEngine::Render
{
    namespace
    {
        /** @brief Runs one pointer range without sharing any output vector between chunks. */
        RenderQueueSet BuildRangeParallel(Jobs::JobSystem& jobs, std::span<const RenderObjectSnapshot* const> objects, const RenderView& view, const RenderResourceView& resources, bool shadowPass, uint32_t grainSize)
        {
            if (objects.empty())
                return {};
            const uint32_t count = static_cast<uint32_t>(objects.size());
            const uint32_t safeGrain = std::max(1u, grainSize);
            const uint32_t chunkCapacity = std::min((count + safeGrain - 1u) / safeGrain, std::max(1u, jobs.GetMaximumParallelChunks()));
            std::vector<RenderQueueSet> chunks(chunkCapacity);
            const Jobs::JobHandle parallel = Jobs::ParallelFor(jobs, count, safeGrain, "Renderer.Packets.Chunk", [&](Jobs::ParallelForRange range) { AppendRenderPackets(chunks[range.chunkIndex], objects.subspan(range.begin, range.end - range.begin), view, resources, shadowPass); });
            try
            {
                jobs.Wait(parallel);
            }
            catch (...)
            {
                jobs.Release(parallel);
                throw;
            }
            jobs.Release(parallel);
            RenderQueueSet result;
            for (RenderQueueSet& chunk : chunks)
                AppendRenderQueueSet(result, std::move(chunk));
            return result;
        }
    } // namespace

    RenderQueueSet BuildRenderPacketsParallel(Jobs::JobSystem& jobs, const VisibilityResult& mainVisibility, const VisibilityResult& shadowVisibility, const RenderView& view, const RenderResourceView& resources, const ParallelPacketConfig& config)
    {
        CHIKA_PROFILE_SCOPE("Renderer.BuildPackets.Parallel");
        const size_t objectCount = mainVisibility.visibleObjects.size() + shadowVisibility.visibleObjects.size();
        if (objectCount < config.minimumObjects)
            return BuildRenderPacketsSerial(mainVisibility, shadowVisibility, view, resources);

        RenderQueueSet result = BuildRangeParallel(jobs, mainVisibility.visibleObjects, view, resources, false, config.grainSize);
        AppendRenderQueueSet(result, BuildRangeParallel(jobs, shadowVisibility.visibleObjects, view, resources, true, config.grainSize));
        return result;
    }
} // namespace ChikaEngine::Render
