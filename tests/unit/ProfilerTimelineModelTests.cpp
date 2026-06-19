#include "ProfilerTimelineModel.hpp"

#include <iostream>

namespace
{
    int g_failures = 0;

    /** @brief Records timeline model failures without depending on an ImGui context. */
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }
} // namespace

/** @brief Verifies timeline clipping, pixel mapping, and exclusive hotspot ordering. */
int main()
{
    ChikaEngine::Profiler::ProfilerFrameCapture capture;
    capture.startNs = 100;
    capture.endNs = 300;
    capture.durationNs = 200;
    capture.zones = {
        { .nameId = 1, .threadId = 1, .startNs = 90, .endNs = 150, .inclusiveNs = 60, .exclusiveNs = 20 },
        { .nameId = 2, .threadId = 1, .startNs = 160, .endNs = 260, .inclusiveNs = 100, .exclusiveNs = 80 },
        { .nameId = 1, .threadId = 2, .startNs = 280, .endNs = 320, .inclusiveNs = 40, .exclusiveNs = 30 },
    };

    const auto rectangles = ChikaEngine::Editor::ProfilerTimelineModel::BuildCpuRects(capture, 100, 200, 1'000.0f);
    Check(rectangles.size() == 3, "clipping must keep every zone intersecting the visible interval");
    Check(rectangles[0].x == 0.0f && rectangles[0].width == 250.0f, "left-clipped zone must map to the viewport origin");
    Check(rectangles[2].x == 900.0f && rectangles[2].width == 100.0f, "right-clipped zone must end at viewport width");

    const auto hotspots = ChikaEngine::Editor::ProfilerTimelineModel::BuildHotspots(capture, 2);
    Check(hotspots.size() == 2, "hotspot result must respect maximum count");
    Check(hotspots[0].nameId == 2 && hotspots[0].exclusiveNs == 80, "hotspots must sort by aggregate exclusive time");
    Check(hotspots[1].nameId == 1 && hotspots[1].callCount == 2, "hotspots must aggregate equal name IDs");

    std::vector<std::shared_ptr<const ChikaEngine::Profiler::ProfilerFrameCapture>> frames;
    for (uint64_t duration = 1; duration <= 100; ++duration)
    {
        auto frame = std::make_shared<ChikaEngine::Profiler::ProfilerFrameCapture>();
        frame->durationNs = duration;
        frames.push_back(frame);
    }
    const auto percentiles = ChikaEngine::Editor::ProfilerTimelineModel::BuildFramePercentiles(frames);
    Check(percentiles.p50Ns == 50 && percentiles.p95Ns == 95 && percentiles.p99Ns == 99, "frame history must use reproducible nearest-rank percentiles");
    return g_failures == 0 ? 0 : 1;
}
