#pragma once

#include "ChikaEngine/profiler/ProfilerFrame.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace ChikaEngine::Editor
{
    struct ProfilerTimelineRect
    {
        size_t zoneIndex = 0;
        uint32_t threadId = 0;
        uint32_t depth = 0;
        float x = 0.0f;
        float width = 0.0f;
    };

    struct ProfilerHotspot
    {
        uint32_t nameId = 0;
        uint64_t inclusiveNs = 0;
        uint64_t exclusiveNs = 0;
        uint32_t callCount = 0;
    };

    struct ProfilerFramePercentiles
    {
        uint64_t p50Ns = 0;
        uint64_t p95Ns = 0;
        uint64_t p99Ns = 0;
    };

    /** @brief Converts immutable profiler captures into renderer-independent timeline geometry. */
    class ProfilerTimelineModel
    {
      public:
        /** @brief Clips CPU zones to a visible interval and maps nanoseconds to pixel coordinates. */
        static std::vector<ProfilerTimelineRect> BuildCpuRects(const Profiler::ProfilerFrameCapture& capture, uint64_t viewStartNs, uint64_t viewDurationNs, float pixelWidth);

        /** @brief Aggregates zones by interned name and returns the highest exclusive CPU costs. */
        static std::vector<ProfilerHotspot> BuildHotspots(const Profiler::ProfilerFrameCapture& capture, size_t maximumCount = 16);

        /** @brief Computes reproducible nearest-rank frame percentiles from completed history only. */
        static ProfilerFramePercentiles BuildFramePercentiles(std::span<const std::shared_ptr<const Profiler::ProfilerFrameCapture>> captures);
    };
} // namespace ChikaEngine::Editor
