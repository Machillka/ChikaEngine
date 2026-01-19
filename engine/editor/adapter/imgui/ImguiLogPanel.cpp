#include "ImguiLogPanel.h"

#include "debug/editor_sink.h"
#include "imgui.h"

#include <cstring>

namespace ChikaEngine::Editor
{
    void ImguiLogPanel::SetEditorSink(ChikaEngine::Debug::EditorLogSink* sink)
    {
        _editorSink = sink;
    }

    void ImguiLogPanel::Clear()
    {
        _lines.clear();
    }

    void ImguiLogPanel::ApplyMessages(const std::vector<ChikaEngine::Debug::MessagePair>& msgs)
    {
        for (const auto& mp : msgs)
        {
            // TODO: 提供更丰富的格式化字符串
            _lines.push_back(mp.second);
        }
    }

    void ImguiLogPanel::FlushFromSink()
    {
        if (!_editorSink)
            return;
        auto msgs = _editorSink->FetchMessages();
        if (!msgs.empty())
            ApplyMessages(msgs);
    }

    void ImguiLogPanel::OnRender(UIContext& ctx)
    {
        FlushFromSink();

        ImGui::Begin(Name());
        if (ImGui::Button("Clear"))
        {
            Clear();
        }

        ImGui::SameLine();
        if (ImGui::Button("Copy"))
        {
            std::string allMsg;
            for (const auto& msg : _lines)
            {
                allMsg += msg + "\n";
            }
            ImGui::SetClipboardText(allMsg.c_str());
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-Scroll", &_autoScroll);
        // 级别过滤按钮 ImGui::SameLine();
        ImGui::Checkbox("Debug", &_showDebug);
        ImGui::SameLine();
        ImGui::Checkbox("Info", &_showInfo);
        ImGui::SameLine();
        ImGui::Checkbox("Warn", &_showWarn);
        ImGui::SameLine();
        ImGui::Checkbox("Error", &_showError);
        ImGui::SameLine();
        ImGui::Checkbox("Fatal", &_showFatal);

        ImGui::Separator();
        char buf[256] = {0};
        if (!_filterText.empty())
        {
            std::strncpy(buf, _filterText.c_str(), sizeof(buf) - 1);
        }
        if (ImGui::InputText("Filter", buf, sizeof(buf)))
        {
            _filterText = std::string(buf);
        }
        ImGui::Separator();
        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& line : _lines)
        {
            // TODO: 保留 level 信息 尝试使用位运算？
            bool passLevel = true;
            if (!_showDebug && line.find("[DEBUG]") != std::string::npos)
                passLevel = false;
            if (!_showInfo && line.find("[INFO]") != std::string::npos)
                passLevel = false;
            if (!_showWarn && line.find("[WARN]") != std::string::npos)
                passLevel = false;
            if (!_showError && line.find("[ERROR]") != std::string::npos)
                passLevel = false;
            if (!_showFatal && line.find("[FATAL]") != std::string::npos)
                passLevel = false;
            if (!passLevel)
                continue;
            if (!_filterText.empty())
            {
                if (line.find(_filterText) == std::string::npos)
                    continue;
            }

            ImGui::TextUnformatted(line.c_str());
        }
        if (_autoScroll)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::End();
    }
} // namespace ChikaEngine::Editor