#pragma once

#include "ChikaEngine/jobs/JobDesc.hpp"
#include "ChikaEngine/jobs/JobSystemCreateInfo.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace ChikaEngine::Jobs
{
    /** @brief Engine-owned CPU scheduler with dependency, wait-help, and explicit shutdown semantics. */
    class JobSystem
    {
      public:
        JobSystem();
        ~JobSystem();

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        /** @brief Allocates bounded queues/storage and starts the configured worker set. */
        bool Initialize(const JobSystemCreateInfo& createInfo = {});

        /** @brief Stops external submission, then drains or cancels accepted work before joining workers. */
        void Shutdown(JobShutdownPolicy policy);

        /** @brief Shuts down with the policy selected at initialization. */
        void Shutdown();

        /** @brief Schedules a fully described job and returns an explicitly owned handle. */
        JobHandle Schedule(JobDesc desc);

        /** @brief Schedules an inline callable without exposing profiler name interning to callers. */
        JobHandle Schedule(std::string_view name, SmallJobFunction function, JobTarget target = JobTarget::AnyWorker);

        /** @brief Schedules a continuation once every dependency reaches a terminal state. */
        JobHandle ScheduleAfter(std::span<const JobHandle> dependencies, JobDesc desc);

        /** @brief Adds work to a parent's completion counter so the parent completes after the child. */
        JobHandle ScheduleChild(JobHandle parent, JobDesc desc);

        template <typename Function> JobHandle Schedule(std::string_view name, Function&& function, JobTarget target = JobTarget::AnyWorker)
        {
            return Schedule(name, SmallJobFunction(std::forward<Function>(function)), target);
        }

        template <typename Function> JobHandle Submit(Function&& function)
        {
            return Schedule("Job", std::forward<Function>(function));
        }

        template <typename Function> JobHandle ScheduleAfter(std::span<const JobHandle> dependencies, std::string_view name, Function&& function, JobFailurePolicy failurePolicy = JobFailurePolicy::Cancel, JobTarget target = JobTarget::AnyWorker)
        {
            JobDesc desc;
            desc.function = SmallJobFunction(std::forward<Function>(function));
            desc.nameId = InternName(name);
            desc.target = target;
            desc.failurePolicy = failurePolicy;
            return ScheduleAfter(dependencies, std::move(desc));
        }

        template <typename Function> JobHandle ScheduleChild(JobHandle parent, std::string_view name, Function&& function, JobTarget target = JobTarget::AnyWorker)
        {
            JobDesc desc;
            desc.function = SmallJobFunction(std::forward<Function>(function));
            desc.nameId = InternName(name);
            desc.target = target;
            return ScheduleChild(parent, std::move(desc));
        }

        /** @brief Waits for completion; scheduler workers and the main thread help execute ready work. */
        void Wait(JobHandle handle);

        /** @brief Tests terminal state without blocking; stale handles return false. */
        bool IsComplete(JobHandle handle) const;

        /** @brief Returns the current state or rejects an invalid generation. */
        JobState GetState(JobHandle handle) const;

        /** @brief Recycles a terminal handle once dependency-owned references are gone. */
        bool Release(JobHandle handle);

        /** @brief Transfers handle cleanup to the scheduler for fire-and-forget work. */
        bool Detach(JobHandle handle);

        /** @brief Executes jobs explicitly targeted at the engine main thread. */
        size_t PumpMainThreadJobs(size_t maximumJobs = SIZE_MAX);

        /** @brief Reports whether worker startup completed and shutdown has not finished. */
        bool IsInitialized() const;

        /** @brief Reports whether external callers may submit new jobs. */
        bool IsAcceptingJobs() const;

        /** @brief Returns scheduler worker count, excluding the main wait-help participant. */
        uint32_t GetWorkerCount() const;

        /** @brief Returns the item threshold below which ParallelFor uses one scheduled callable. */
        uint32_t GetParallelForMinItems() const;

        /** @brief Returns the configured upper bound for ParallelFor chunk creation. */
        uint32_t GetMaximumParallelChunks() const;

        /** @brief Returns a lock-free snapshot of scheduler counters. */
        JobSystemStatistics GetStatistics() const;

        /** @brief Applies the engine thread budget while preserving at least one worker. */
        static uint32_t ResolveWorkerCount(uint32_t requestedWorkers, uint32_t reservedThreads);

      private:
        static uint32_t InternName(std::string_view name);

        class Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace ChikaEngine::Jobs
