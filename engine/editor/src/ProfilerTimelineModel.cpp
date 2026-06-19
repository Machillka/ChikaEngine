#include "ProfilerTimelineModel.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ChikaEngine::Editor
{
    std::vector<ProfilerTimelineRect> ProfilerTimelineModel::BuildCpuRects(const Profiler::ProfilerFrameCapture& capture, uint64_t viewStartNs, uint64_t viewDurationNs, float pixelWidth)
    {
        std::vector<ProfilerTimelineRect> result;
        if (viewDurationNs == 0 || pixelWidth <= 0.0f)
            return result;

        const uint64_t viewEndNs = viewStartNs + viewDurationNs;
        result.reserve(capture.zones.size());
        for (size_t index = 0; index < capture.zones.size(); ++index)
        {
            const Profiler::ProfilerZone& zone = capture.zones[index];
            if (zone.endNs <= viewStartNs || zone.startNs >= viewEndNs)
                continue;
            const uint64_t clippedStart = std::max(zone.startNs, viewStartNs);
            const uint64_t clippedEnd = std::min(zone.endNs, viewEndNs);
            result.push_back({
                .zoneIndex = index,
                .threadId = zone.threadId,
                .depth = zone.depth,
                .x = static_cast<float>(clippedStart - viewStartNs) / static_cast<float>(viewDurationNs) * pixelWidth,
                .width = std::max(1.0f, static_cast<float>(clippedEnd - clippedStart) / static_cast<float>(viewDurationNs) * pixelWidth),
            });
        }
        return result;
    }

    std::vector<ProfilerHotspot> ProfilerTimelineModel::BuildHotspots(const Profiler::ProfilerFrameCapture& capture, size_t maximumCount)
    {
        std::unordered_map<uint32_t, ProfilerHotspot> aggregated;
        for (const Profiler::ProfilerZone& zone : capture.zones)
        {
            ProfilerHotspot& hotspot = aggregated[zone.nameId];
            hotspot.nameId = zone.nameId;
            hotspot.inclusiveNs += zone.inclusiveNs;
            hotspot.exclusiveNs += zone.exclusiveNs;
            ++hotspot.callCount;
        }

        std::vector<ProfilerHotspot> result;
        result.reserve(aggregated.size());
        for (const auto& [nameId, hotspot] : aggregated)
            result.push_back(hotspot);
        std::ranges::sort(result, [](const ProfilerHotspot& lhs, const ProfilerHotspot& rhs) { return lhs.exclusiveNs > rhs.exclusiveNs; });
        if (result.size() > maximumCount)
            result.resize(maximumCount);
        return result;
    }

    ProfilerFramePercentiles ProfilerTimelineModel::BuildFramePercentiles(std::span<const std::shared_ptr<const Profiler::ProfilerFrameCapture>> captures)
    {
        std::vector<uint64_t> durations;
        durations.reserve(captures.size());
        for (const auto& capture : captures)
        {
            if (capture)
                durations.push_back(capture->durationNs);
        }
        if (durations.empty())
            return {};
        std::ranges::sort(durations);
        const auto percentile = [&durations](double probability)
        {
            const size_t rank = std::max<size_t>(1, static_cast<size_t>(std::ceil(probability * static_cast<double>(durations.size()))));
            return durations[std::min(rank - 1, durations.size() - 1)];
        };
        return { percentile(0.50), percentile(0.95), percentile(0.99) };
    }
} // namespace ChikaEngine::Editor
