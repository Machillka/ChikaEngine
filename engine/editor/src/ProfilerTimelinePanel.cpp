#include "ProfilerTimelinePanel.hpp"

#include "ChikaEngine/profiler/ProfilerName.hpp"
#include "ChikaEngine/profiler/ProfilerSession.hpp"
#include "ChikaEngine/profiler/ProfilerTraceExporter.hpp"
#include "ProfilerTimelineModel.hpp"

#include <algorithm>
#include <imgui.h>
#include <unordered_map>

namespace ChikaEngine::Editor
{
    namespace
    {
#if defined(CHIKA_DEBUG)
        constexpr const char* kProfilerBuildType = "Debug";
#else
        constexpr const char* kProfilerBuildType = "Release";
#endif
#if defined(_WIN32)
        constexpr const char* kProfilerPlatform = "Windows";
#elif defined(__APPLE__)
        constexpr const char* kProfilerPlatform = "macOS";
#else
        constexpr const char* kProfilerPlatform = "Linux";
#endif

        /** @brief Produces stable colors from name IDs so repeated zones remain visually recognizable. */
        ImU32 ZoneColor(uint32_t nameId, bool malformed)
        {
            if (malformed)
                return IM_COL32(220, 70, 70, 255);
            const uint32_t hash = nameId * 2'654'435'761u;
            return IM_COL32(70 + (hash & 0x7f), 80 + ((hash >> 8u) & 0x7f), 100 + ((hash >> 16u) & 0x7f), 255);
        }

        /** @brief Formats nanosecond durations in milliseconds for compact UI labels. */
        double ToMilliseconds(uint64_t nanoseconds)
        {
            return static_cast<double>(nanoseconds) / 1'000'000.0;
        }
    } // namespace

    void ProfilerTimelinePanel::Initialize(EditorContext* context)
    {
        _context = context;
    }

    void ProfilerTimelinePanel::Tick(float)
    {
        m_frames = Profiler::ProfilerSession::Get().GetHistory().Snapshot();
        if (m_followLatest && !m_frames.empty())
            m_selected = m_frames.back();
        else if (m_selected && !Profiler::ProfilerSession::Get().GetHistory().Find(m_selected->frameId))
            m_selected = m_frames.empty() ? nullptr : m_frames.back();
    }

    void ProfilerTimelinePanel::OnImGuiRender()
    {
        if (!ImGui::Begin(GetName().c_str(), &_isActive))
        {
            ImGui::End();
            return;
        }

        Profiler::ProfilerSession& session = Profiler::ProfilerSession::Get();
        bool enabled = session.IsEnabled();
        if (ImGui::Checkbox("Capture", &enabled))
            session.SetEnabled(enabled);
        ImGui::SameLine();
        ImGui::Checkbox("Follow latest", &m_followLatest);
        ImGui::SameLine();
        if (ImGui::Button("Export Perfetto"))
        {
            std::string error;
            Profiler::ProfilerTraceMetadata metadata{
                .applicationName = "ChikaEditor",
                .buildType = kProfilerBuildType,
                .platform = kProfilerPlatform,
            };
            if (_context && _context->renderer && _context->renderer->GetRHIHandle())
                metadata.additional["gpu"] = _context->renderer->GetRHIHandle()->GetDeviceName();
            m_exportStatus = Profiler::ProfilerTraceExporter::Export(".chika/profiler/editor-capture.json", m_frames, metadata, &error) ? "Exported .chika/profiler/editor-capture.json" : error;
        }
        if (!m_exportStatus.empty())
            ImGui::TextDisabled("%s", m_exportStatus.c_str());

        if (m_frames.empty())
        {
            ImGui::TextDisabled("No completed profiler frames");
            ImGui::End();
            return;
        }

        std::vector<float> frameTimes;
        frameTimes.reserve(m_frames.size());
        for (const auto& frame : m_frames)
            frameTimes.push_back(static_cast<float>(ToMilliseconds(frame->durationNs)));
        ImGui::PlotLines("Frame history (ms)", frameTimes.data(), static_cast<int>(frameTimes.size()), 0, nullptr, 0.0f, 50.0f, ImVec2(0.0f, 70.0f));
        const ProfilerFramePercentiles percentiles = ProfilerTimelineModel::BuildFramePercentiles(m_frames);
        ImGui::Text("p50 %.3f ms   p95 %.3f ms   p99 %.3f ms", ToMilliseconds(percentiles.p50Ns), ToMilliseconds(percentiles.p95Ns), ToMilliseconds(percentiles.p99Ns));

        int selectedIndex = static_cast<int>(m_frames.size() - 1);
        if (m_selected)
        {
            const auto selected = std::ranges::find(m_frames, m_selected->frameId, [](const auto& frame) { return frame->frameId; });
            if (selected != m_frames.end())
                selectedIndex = static_cast<int>(std::distance(m_frames.begin(), selected));
        }
        if (ImGui::SliderInt("Frame", &selectedIndex, 0, static_cast<int>(m_frames.size() - 1)))
        {
            m_followLatest = false;
            m_selected = m_frames[static_cast<size_t>(selectedIndex)];
        }

        if (!m_selected)
        {
            ImGui::End();
            return;
        }
        ImGui::Text("Frame %llu  %.3f ms", static_cast<unsigned long long>(m_selected->frameId), ToMilliseconds(m_selected->durationNs));
        ImGui::SameLine();
        if (ImGui::Button("Pin"))
            session.GetHistory().Pin(m_selected->frameId);
        if (m_selected->droppedEventCount > 0 || m_selected->malformedZoneCount > 0)
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Dropped: %llu  Malformed: %u", static_cast<unsigned long long>(m_selected->droppedEventCount), m_selected->malformedZoneCount);

        ImGui::SliderFloat("Zoom", &m_zoom, 1.0f, 32.0f, "%.1fx", ImGuiSliderFlags_Logarithmic);
        const float maximumPan = std::max(0.0f, 1.0f - 1.0f / m_zoom);
        ImGui::SliderFloat("Pan", &m_pan, 0.0f, maximumPan, "%.3f");
        DrawTimeline(*m_selected);
        DrawHotspots(*m_selected);
        ImGui::End();
    }

    void ProfilerTimelinePanel::DrawTimeline(const Profiler::ProfilerFrameCapture& capture)
    {
        constexpr float rowHeight = 20.0f;
        const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const uint64_t viewDuration = std::max<uint64_t>(1, static_cast<uint64_t>(static_cast<double>(capture.durationNs) / m_zoom));
        const uint64_t viewStart = capture.startNs + static_cast<uint64_t>(static_cast<double>(capture.durationNs) * m_pan);
        const auto rectangles = ProfilerTimelineModel::BuildCpuRects(capture, viewStart, viewDuration, width);

        std::unordered_map<uint32_t, uint32_t> laneByThread;
        uint32_t lane = 0;
        uint32_t maximumDepth = 0;
        for (const auto& thread : capture.threads)
            laneByThread[thread.threadId] = lane++;
        for (const auto& rectangle : rectangles)
            maximumDepth = std::max(maximumDepth, rectangle.depth);
        const float cpuHeight = std::max(rowHeight, static_cast<float>(lane * (maximumDepth + 2u)) * rowHeight);
        const float gpuHeight = capture.gpuZones.empty() ? 0.0f : rowHeight * 2.0f;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("ProfilerTimelineCanvas", ImVec2(width, cpuHeight + gpuHeight), ImGuiButtonFlags_MouseButtonLeft);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + cpuHeight + gpuHeight), IM_COL32(25, 27, 32, 255));

        for (const auto& rectangle : rectangles)
        {
            const Profiler::ProfilerZone& zone = capture.zones[rectangle.zoneIndex];
            const float y = origin.y + static_cast<float>(laneByThread[rectangle.threadId] * (maximumDepth + 2u) + rectangle.depth + 1u) * rowHeight;
            const ImVec2 minimum(origin.x + rectangle.x, y);
            const ImVec2 maximum(minimum.x + rectangle.width, y + rowHeight - 2.0f);
            drawList->AddRectFilled(minimum, maximum, ZoneColor(zone.nameId, zone.malformed), 2.0f);
            if (rectangle.width > 45.0f)
            {
                const std::string name = Profiler::ProfilerNameRegistry::Instance().Resolve(zone.nameId);
                drawList->PushClipRect(minimum, maximum, true);
                drawList->AddText(ImVec2(minimum.x + 3.0f, minimum.y + 2.0f), IM_COL32_WHITE, name.c_str());
                drawList->PopClipRect();
            }
            if (ImGui::IsMouseHoveringRect(minimum, maximum))
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(Profiler::ProfilerNameRegistry::Instance().Resolve(zone.nameId).c_str());
                ImGui::Text("Inclusive %.3f ms", ToMilliseconds(zone.inclusiveNs));
                ImGui::Text("Exclusive %.3f ms", ToMilliseconds(zone.exclusiveNs));
                ImGui::EndTooltip();
            }
        }

        uint64_t gpuCursor = 0;
        for (const Profiler::ProfilerGpuZone& zone : capture.gpuZones)
        {
            const float x = static_cast<float>(gpuCursor) / static_cast<float>(viewDuration) * width;
            const float zoneWidth = std::max(1.0f, static_cast<float>(zone.durationNs) / static_cast<float>(viewDuration) * width);
            const float y = origin.y + cpuHeight + rowHeight * 0.25f;
            drawList->AddRectFilled(ImVec2(origin.x + x, y), ImVec2(origin.x + x + zoneWidth, y + rowHeight - 2.0f), ZoneColor(zone.nameId, !zone.valid), 2.0f);
            gpuCursor += zone.durationNs;
        }
    }

    void ProfilerTimelinePanel::DrawHotspots(const Profiler::ProfilerFrameCapture& capture)
    {
        if (!ImGui::CollapsingHeader("CPU Hotspots", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        if (ImGui::BeginTable("ProfilerHotspots", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Zone");
            ImGui::TableSetupColumn("Exclusive ms");
            ImGui::TableSetupColumn("Inclusive ms");
            ImGui::TableSetupColumn("Calls");
            ImGui::TableHeadersRow();
            for (const ProfilerHotspot& hotspot : ProfilerTimelineModel::BuildHotspots(capture))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(Profiler::ProfilerNameRegistry::Instance().Resolve(hotspot.nameId).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", ToMilliseconds(hotspot.exclusiveNs));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", ToMilliseconds(hotspot.inclusiveNs));
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", hotspot.callCount);
            }
            ImGui::EndTable();
        }
    }
} // namespace ChikaEngine::Editor
