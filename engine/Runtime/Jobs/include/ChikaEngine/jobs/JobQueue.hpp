#pragma once

#include "ChikaEngine/jobs/JobTypes.hpp"

#include <cstddef>
#include <deque>
#include <mutex>

namespace ChikaEngine::Jobs
{
    /** @brief Bounded correctness-first deque supporting local LIFO and remote FIFO stealing. */
    class JobQueue
    {
      public:
        explicit JobQueue(size_t capacity = 4'096);

        /** @brief Appends a handle when bounded capacity is available. */
        bool Push(JobHandle handle);

        /** @brief Pops newest work for cache-friendly local execution. */
        bool PopLocal(JobHandle& handle);

        /** @brief Removes oldest work so remote workers do not fight the owner's hot end. */
        bool Steal(JobHandle& handle);

        /** @brief Returns queue depth under the queue mutex. */
        size_t Size() const;

        /** @brief Discards queue entries during scheduler teardown. */
        void Clear();

      private:
        size_t m_capacity = 0;
        mutable std::mutex m_mutex;
        std::deque<JobHandle> m_queue;
    };
} // namespace ChikaEngine::Jobs
