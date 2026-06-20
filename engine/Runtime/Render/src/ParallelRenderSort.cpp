#include "ChikaEngine/ParallelRenderSort.hpp"

#include "ChikaEngine/RenderSortKey.hpp"
#include "ChikaEngine/jobs/ParallelFor.hpp"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"

#include <algorithm>

namespace ChikaEngine::Render
{
    namespace
    {
        struct SortedRun
        {
            uint32_t begin = 0;
            uint32_t end = 0;
            bool valid = false;
        };

        /** @brief Uses the same explicit key as the serial oracle. */
        bool OpaqueLess(const RenderPacket& lhs, const RenderPacket& rhs)
        {
            return BuildOpaqueSortKey(lhs) < BuildOpaqueSortKey(rhs);
        }
    } // namespace

    void SortAndBuildRenderBatchesParallel(Jobs::JobSystem& jobs, RenderQueue& queue, const ParallelSortConfig& config)
    {
        if (queue.packets.size() < config.minimumPackets)
        {
            SortAndBuildRenderBatches(queue, false);
            return;
        }

        CHIKA_PROFILE_SCOPE("Renderer.Sort.Parallel");
        const uint32_t count = static_cast<uint32_t>(queue.packets.size());
        const uint32_t grain = std::max(1u, config.grainSize);
        const uint32_t runCapacity = std::min((count + grain - 1u) / grain, std::max(1u, jobs.GetMaximumParallelChunks()));
        std::vector<SortedRun> runs(runCapacity);
        const Jobs::JobHandle parallel = Jobs::ParallelFor(jobs,
                                                           count,
                                                           grain,
                                                           "Renderer.Sort.Chunk",
                                                           [&](Jobs::ParallelForRange range)
                                                           {
                                                               std::stable_sort(queue.packets.begin() + range.begin, queue.packets.begin() + range.end, OpaqueLess);
                                                               runs[range.chunkIndex] = { .begin = range.begin, .end = range.end, .valid = true };
                                                           });
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

        std::vector<SortedRun> activeRuns;
        for (const SortedRun& run : runs)
        {
            if (run.valid)
                activeRuns.push_back(run);
        }
        while (activeRuns.size() > 1)
        {
            std::vector<SortedRun> merged;
            merged.reserve((activeRuns.size() + 1u) / 2u);
            for (size_t index = 0; index < activeRuns.size(); index += 2)
            {
                if (index + 1 >= activeRuns.size())
                {
                    merged.push_back(activeRuns[index]);
                    continue;
                }
                const SortedRun left = activeRuns[index];
                const SortedRun right = activeRuns[index + 1];
                std::inplace_merge(queue.packets.begin() + left.begin, queue.packets.begin() + left.end, queue.packets.begin() + right.end, OpaqueLess);
                merged.push_back({ .begin = left.begin, .end = right.end, .valid = true });
            }
            activeRuns = std::move(merged);
        }
        BuildRenderBatches(queue);
    }

    void SortAndBuildRenderQueueSetParallel(Jobs::JobSystem& jobs, RenderQueueSet& queues, const ParallelSortConfig& config)
    {
        SortAndBuildRenderBatchesParallel(jobs, queues.shadow, config);
        SortAndBuildRenderBatchesParallel(jobs, queues.forwardOpaque, config);
        SortAndBuildRenderBatchesParallel(jobs, queues.gbufferOpaque, config);
        SortAndBuildRenderBatches(queues.forwardTransparent, true);
    }
} // namespace ChikaEngine::Render
