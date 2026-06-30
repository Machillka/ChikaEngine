#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace ChikaEngine::Render
{
    struct GpuVisibilityDiff
    {
        std::vector<uint32_t> missingFromGpu;
        std::vector<uint32_t> extraOnGpu;
        bool matches = true;
    };

    struct GpuVisibilityReadback
    {
        uint64_t frameId = 0;
        std::vector<uint32_t> visibleObjectIds;
        bool valid = false;
    };

    /**
     * @brief Tracks delayed GPU readback ownership by frame ID without blocking the current frame.
     */
    class GpuVisibilityValidationQueue
    {
      public:
        /** @brief Registers a CPU oracle snapshot for a frame that will receive delayed GPU readback. */
        void EnqueueCpuOracle(uint64_t frameId, std::vector<uint32_t> visibleObjectIds);
        /** @brief Matches one readback packet to its original CPU oracle and returns the diff. */
        GpuVisibilityDiff ConsumeReadback(const GpuVisibilityReadback& readback);
        /** @brief Drops pending validation work when the camera, scene or mode changes. */
        void Clear();
        uint32_t PendingCount() const;

      private:
        std::deque<GpuVisibilityReadback> m_pendingCpuOracles;
    };

    /** @brief Compares two unordered visibility ID sets and reports deterministic missing/extra lists. */
    GpuVisibilityDiff CompareVisibilitySets(std::vector<uint32_t> cpuVisibleObjectIds, std::vector<uint32_t> gpuVisibleObjectIds);
} // namespace ChikaEngine::Render
