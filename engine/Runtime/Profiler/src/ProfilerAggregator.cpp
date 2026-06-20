#include "ChikaEngine/profiler/ProfilerAggregator.hpp"

#include <algorithm>
#include <cmath>

namespace ChikaEngine::Profiler
{
    std::shared_ptr<const ProfilerFrameCapture> ProfilerAggregator::Aggregate(uint64_t frameId, uint64_t frameStartNs, uint64_t frameEndNs, std::vector<ProfilerThreadBatch> batches, uint64_t hitchThresholdNs)
    {
        for (auto& batch : batches)
        {
            for (const ProfilerEvent& event : batch.events)
            {
                PendingThread& pending = m_pending[event.frameId][batch.threadId];
                pending.name = batch.threadName;
                pending.events.push_back(event);
            }
            PendingThread& current = m_pending[frameId][batch.threadId];
            current.name = batch.threadName;
            current.dropped += batch.droppedEventCount;
        }

        auto capture = std::make_shared<ProfilerFrameCapture>();
        capture->frameId = frameId;
        capture->startNs = frameStartNs;
        capture->endNs = std::max(frameStartNs, frameEndNs);
        capture->durationNs = capture->endNs - capture->startNs;
        capture->hitch = hitchThresholdNs > 0 && capture->durationNs >= hitchThresholdNs;

        auto pendingFrame = m_pending.find(frameId);
        if (pendingFrame == m_pending.end())
            return capture;

        for (auto& [threadId, thread] : pendingFrame->second)
        {
            capture->threads.push_back({ threadId, thread.name });
            capture->droppedEventCount += thread.dropped;
            std::ranges::stable_sort(thread.events, {}, &ProfilerEvent::timestampNs);

            struct OpenZone
            {
                size_t index = 0;
                uint64_t childDurationNs = 0;
            };
            std::vector<OpenZone> stack;
            const auto closeTop = [&](uint64_t endNs, bool malformed)
            {
                OpenZone open = stack.back();
                stack.pop_back();
                ProfilerZone& zone = capture->zones[open.index];
                zone.endNs = std::max(zone.startNs, endNs);
                zone.inclusiveNs = zone.endNs - zone.startNs;
                zone.exclusiveNs = zone.inclusiveNs > open.childDurationNs ? zone.inclusiveNs - open.childDurationNs : 0;
                zone.malformed = zone.malformed || malformed;
                if (zone.malformed)
                    ++capture->malformedZoneCount;
                if (!stack.empty())
                    stack.back().childDurationNs += zone.inclusiveNs;
            };

            for (const ProfilerEvent& event : thread.events)
            {
                switch (event.type)
                {
                case ProfilerEventType::ZoneBegin:
                    capture->zones.push_back({ .nameId = event.nameId, .threadId = threadId, .depth = static_cast<uint32_t>(stack.size()), .startNs = event.timestampNs });
                    stack.push_back({ capture->zones.size() - 1, 0 });
                    break;
                case ProfilerEventType::ZoneEnd:
                    while (!stack.empty() && capture->zones[stack.back().index].nameId != event.nameId)
                        closeTop(event.timestampNs, true);
                    if (stack.empty())
                    {
                        ++capture->malformedZoneCount;
                        break;
                    }
                    closeTop(event.timestampNs, false);
                    break;
                case ProfilerEventType::Counter:
                {
                    const bool floatingPoint = (static_cast<uint8_t>(event.flags) & static_cast<uint8_t>(ProfilerEventFlags::FloatingPointPayload)) != 0;
                    capture->counters.push_back({ event.nameId, threadId, event.timestampNs, floatingPoint ? UnpackProfilerDouble(event.payload) : static_cast<double>(UnpackProfilerInteger(event.payload)) });
                    break;
                }
                case ProfilerEventType::Instant:
                    capture->instants.push_back({ event.nameId, threadId, event.timestampNs, event.payload });
                    break;
                default:
                    break;
                }
            }
            while (!stack.empty())
                closeTop(capture->endNs, true);
        }

        std::ranges::sort(capture->threads, {}, &ProfilerThreadInfo::threadId);
        m_pending.erase(pendingFrame);
        return capture;
    }

    void ProfilerAggregator::Clear()
    {
        m_pending.clear();
    }
} // namespace ChikaEngine::Profiler
