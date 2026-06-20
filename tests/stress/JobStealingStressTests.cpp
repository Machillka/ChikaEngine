#include "ChikaEngine/jobs/JobSystem.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    namespace Jobs = ChikaEngine::Jobs;
    constexpr uint32_t taskCount = 1'000'000;
    constexpr uint32_t batchSize = 32'768;

    Jobs::JobSystem jobs;
    if (!jobs.Initialize({ .workerCount = 8, .jobCapacity = 65'536, .injectionQueueCapacity = 65'536 }))
        return 1;
    std::vector<std::atomic<uint8_t>> visits(taskCount);
    std::vector<Jobs::JobHandle> handles;
    handles.reserve(batchSize);
    for (uint32_t batchBegin = 0; batchBegin < taskCount; batchBegin += batchSize)
    {
        const uint32_t batchEnd = std::min(taskCount, batchBegin + batchSize);
        handles.clear();
        for (uint32_t index = batchBegin; index < batchEnd; ++index)
        {
            const Jobs::JobHandle handle = jobs.Schedule("Stress.ShortTask", [&, index] { visits[index].fetch_add(1, std::memory_order_relaxed); });
            if (!handle.IsValid())
                return 2;
            handles.push_back(handle);
        }
        for (Jobs::JobHandle handle : handles)
        {
            jobs.Wait(handle);
            if (!jobs.Release(handle))
                return 3;
        }
    }
    for (const auto& visit : visits)
    {
        if (visit.load(std::memory_order_relaxed) != 1)
            return 4;
    }
    if (jobs.GetStatistics().completedJobs != taskCount)
        return 5;
    jobs.Shutdown();
    std::cout << "One million job stress checks passed\n";
    return 0;
}
