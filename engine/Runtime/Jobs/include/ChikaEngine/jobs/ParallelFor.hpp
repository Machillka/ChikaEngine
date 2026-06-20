#pragma once

#include "ChikaEngine/jobs/JobSystem.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ChikaEngine::Jobs
{
    namespace Detail
    {
        /** @brief Waits and releases partially submitted chunks before reporting a construction failure. */
        inline void CleanupParallelChunks(JobSystem& jobs, std::span<const JobHandle> chunks) noexcept
        {
            for (JobHandle chunk : chunks)
            {
                try
                {
                    jobs.Wait(chunk);
                }
                catch (...)
                {
                }
                jobs.Release(chunk);
            }
        }
    } // namespace Detail

    struct ParallelForRange
    {
        uint32_t begin = 0;
        uint32_t end = 0;
        uint32_t chunkIndex = 0;
    };

    /** @brief Splits an index range into bounded chunks and returns a dependency join handle. */
    template <typename Function> JobHandle ParallelFor(JobSystem& jobs, uint32_t count, uint32_t grainSize, std::string_view name, Function&& function)
    {
        using StoredFunction = std::remove_cvref_t<Function>;
        auto sharedFunction = std::make_shared<StoredFunction>(std::forward<Function>(function));
        const uint32_t safeGrain = std::max(1u, grainSize);
        if (count == 0 || count <= jobs.GetParallelForMinItems() || jobs.GetWorkerCount() <= 1)
        {
            return jobs.Schedule(name,
                                 [sharedFunction, count]()
                                 {
                                     if (count > 0)
                                         (*sharedFunction)(ParallelForRange{ 0, count, 0 });
                                 });
        }

        uint32_t chunkCount = (count + safeGrain - 1u) / safeGrain;
        chunkCount = std::min(chunkCount, jobs.GetMaximumParallelChunks());
        const uint32_t effectiveGrain = (count + chunkCount - 1u) / chunkCount;
        std::vector<JobHandle> chunks;
        chunks.reserve(chunkCount);
        for (uint32_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
        {
            const uint32_t begin = chunkIndex * effectiveGrain;
            const uint32_t end = std::min(count, begin + effectiveGrain);
            if (begin >= end)
                break;
            JobHandle chunk = jobs.Schedule(name, [sharedFunction, begin, end, chunkIndex]() { (*sharedFunction)(ParallelForRange{ begin, end, chunkIndex }); });
            if (!chunk.IsValid())
            {
                Detail::CleanupParallelChunks(jobs, chunks);
                throw JobCapacityError("failed to schedule ParallelFor chunk");
            }
            chunks.push_back(chunk);
        }

        JobHandle join = jobs.ScheduleAfter(chunks, "ParallelFor.Join", [] {}, JobFailurePolicy::Propagate);
        if (!join.IsValid())
        {
            Detail::CleanupParallelChunks(jobs, chunks);
            throw JobCapacityError("failed to schedule ParallelFor join");
        }
        for (JobHandle chunk : chunks)
            jobs.Detach(chunk);
        return join;
    }
} // namespace ChikaEngine::Jobs
