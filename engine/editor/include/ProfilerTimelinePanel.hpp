#pragma once

#include "ChikaEngine/profiler/ProfilerFrame.hpp"
#include "IEditorPanel.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ChikaEngine::Editor
{
    /** @brief Presents immutable CPU/GPU captures without reaching into producer thread buffers. */
    class ProfilerTimelinePanel final : public IEditorPanel
    {
      public:
        /** @brief Stores the editor context; profiler data comes from the independent runtime service. */
        void Initialize(EditorContext* context) override;

        /** @brief Selects the newest completed capture while live-follow mode is enabled. */
        void Tick(float deltaTime) override;

        /** @brief Draws frame history, zoomable CPU/GPU lanes, counters, and hotspot summaries. */
        void OnImGuiRender() override;

        const std::string& GetName() const override
        {
            static const std::string name = "Profiler";
            return name;
        }

      private:
        void DrawTimeline(const Profiler::ProfilerFrameCapture& capture);
        void DrawHotspots(const Profiler::ProfilerFrameCapture& capture);

        std::vector<std::shared_ptr<const Profiler::ProfilerFrameCapture>> m_frames;
        std::shared_ptr<const Profiler::ProfilerFrameCapture> m_selected;
        bool m_followLatest = true;
        float m_zoom = 1.0f;
        float m_pan = 0.0f;
        std::string m_exportStatus;
    };
} // namespace ChikaEngine::Editor
