#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ChikaEngine::Profiler
{
    struct ProfilerThreadInfo
    {
        uint32_t threadId = 0;
        std::string name;
    };

    struct ProfilerZone
    {
        uint32_t nameId = 0;
        uint32_t threadId = 0;
        uint32_t depth = 0;
        uint64_t startNs = 0;
        uint64_t endNs = 0;
        uint64_t inclusiveNs = 0;
        uint64_t exclusiveNs = 0;
        bool malformed = false;
    };

    struct ProfilerCounter
    {
        uint32_t nameId = 0;
        uint32_t threadId = 0;
        uint64_t timestampNs = 0;
        double value = 0.0;
    };

    struct ProfilerInstant
    {
        uint32_t nameId = 0;
        uint32_t threadId = 0;
        uint64_t timestampNs = 0;
    };

    struct ProfilerGpuZone
    {
        uint32_t nameId = 0;
        uint32_t queueId = 0;
        uint64_t durationNs = 0;
        bool valid = true;
    };

    /** @brief Immutable presentation model produced after all CPU events for a frame are drained. */
    struct ProfilerFrameCapture
    {
        uint64_t frameId = 0;
        uint64_t startNs = 0;
        uint64_t endNs = 0;
        uint64_t durationNs = 0;
        std::vector<ProfilerThreadInfo> threads;
        std::vector<ProfilerZone> zones;
        std::vector<ProfilerCounter> counters;
        std::vector<ProfilerInstant> instants;
        std::vector<ProfilerGpuZone> gpuZones;
        uint64_t droppedEventCount = 0;
        uint32_t malformedZoneCount = 0;
        bool hitch = false;
    };

    struct ProfilerGpuTiming
    {
        uint64_t frameId = 0;
        std::string name;
        uint32_t queueId = 0;
        uint64_t durationNs = 0;
        bool valid = true;
    };
} // namespace ChikaEngine::Profiler
