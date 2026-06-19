#pragma once

#include "ChikaEngine/profiler/ProfilerFrame.hpp"
#include "ChikaEngine/profiler/ProfilerThreadRegistry.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ChikaEngine::Profiler
{
    /** @brief Reconstructs nested CPU zones from drained thread batches without touching producers. */
    class ProfilerAggregator
    {
      public:
        std::shared_ptr<const ProfilerFrameCapture> Aggregate(uint64_t frameId, uint64_t frameStartNs, uint64_t frameEndNs, std::vector<ProfilerThreadBatch> batches, uint64_t hitchThresholdNs);
        void Clear();

      private:
        struct PendingThread
        {
            std::string name;
            std::vector<ProfilerEvent> events;
            uint64_t dropped = 0;
        };

        std::unordered_map<uint64_t, std::unordered_map<uint32_t, PendingThread>> m_pending;
    };
} // namespace ChikaEngine::Profiler
