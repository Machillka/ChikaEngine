#pragma once

#include "ChikaEngine/jobs/JobDesc.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

namespace ChikaEngine::Jobs
{
    namespace Detail
    {
        struct JobSlot
        {
            std::atomic<uint32_t> generation{ 1 };
            std::atomic<JobState> state{ JobState::Free };
            std::atomic<uint32_t> remainingDependencies{ 0 };
            std::atomic<uint32_t> unfinishedWork{ 0 };
            std::atomic<uint32_t> internalReferences{ 0 };
            std::atomic<bool> dependencyFailed{ false };
            std::atomic<bool> ownContributionFinished{ false };
            std::atomic<bool> setupComplete{ false };
            std::atomic<bool> finalizing{ false };
            std::atomic<bool> autoRelease{ false };
            SmallJobFunction function;
            JobHandle parent;
            JobTarget target = JobTarget::AnyWorker;
            JobFailurePolicy failurePolicy = JobFailurePolicy::Cancel;
            uint32_t nameId = 0;
            uint64_t enqueueTimestampNs = 0;
            JobState desiredTerminalState = JobState::Completed;
            std::exception_ptr exception;
            std::vector<JobHandle> dependents;
            mutable std::mutex mutex;
            std::condition_variable condition;
        };
    } // namespace Detail

    /** @brief Preallocated slot pool that invalidates recycled handles by generation. */
    class JobStorage
    {
      public:
        explicit JobStorage(size_t capacity);
        ~JobStorage();

        JobStorage(const JobStorage&) = delete;
        JobStorage& operator=(const JobStorage&) = delete;

        /** @brief Claims and initializes one preallocated slot without per-job heap allocation. */
        JobHandle Allocate(JobDesc desc);

        /** @brief Recycles a terminal unreferenced slot and advances its generation. */
        bool Release(JobHandle handle);

        /** @brief Checks both slot index and generation. */
        bool IsValid(JobHandle handle) const;
        size_t Capacity() const;
        size_t ActiveCount() const;

        /** @brief Resolves a process-local handle; callers must obey scheduler lifetime locking. */
        Detail::JobSlot* Resolve(JobHandle handle);
        const Detail::JobSlot* Resolve(JobHandle handle) const;

        template <typename Function> void ForEachActive(Function&& function)
        {
            for (size_t index = 0; index < m_capacity; ++index)
            {
                Detail::JobSlot& slot = m_slots[index];
                if (slot.state.load(std::memory_order_acquire) != JobState::Free)
                    function(JobHandle{ static_cast<uint32_t>(index), slot.generation.load(std::memory_order_acquire) }, slot);
            }
        }

      private:
        size_t m_capacity = 0;
        std::unique_ptr<Detail::JobSlot[]> m_slots;
        mutable std::mutex m_freeMutex;
        std::vector<uint32_t> m_freeIndices;
        std::atomic<size_t> m_activeCount{ 0 };
    };
} // namespace ChikaEngine::Jobs
