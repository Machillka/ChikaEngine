#pragma once

#include "ChikaEngine/profiler/ProfilerFrame.hpp"

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ChikaEngine::Profiler
{
    /** @brief Bounded immutable frame history with explicit pinning for captures that must survive eviction. */
    class ProfilerHistory
    {
      public:
        explicit ProfilerHistory(size_t capacity = 300);

        void SetCapacity(size_t capacity);
        void Add(std::shared_ptr<const ProfilerFrameCapture> capture);
        bool Replace(std::shared_ptr<const ProfilerFrameCapture> capture);
        std::shared_ptr<const ProfilerFrameCapture> Latest() const;
        std::shared_ptr<const ProfilerFrameCapture> Find(uint64_t frameId) const;
        std::vector<std::shared_ptr<const ProfilerFrameCapture>> Snapshot() const;
        bool Pin(uint64_t frameId);
        void Unpin(uint64_t frameId);
        void Clear();

      private:
        void TrimLocked();

        mutable std::mutex m_mutex;
        size_t m_capacity = 300;
        std::deque<std::shared_ptr<const ProfilerFrameCapture>> m_frames;
        std::unordered_map<uint64_t, std::shared_ptr<const ProfilerFrameCapture>> m_pinned;
    };
} // namespace ChikaEngine::Profiler
