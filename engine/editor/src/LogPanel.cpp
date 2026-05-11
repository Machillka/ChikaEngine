#include "LogPanel.hpp"
#include "ChikaEngine/debug/log_system.h"
#include <imgui.h>
#include <memory>
#include <utility>

namespace ChikaEngine::Editor
{
    LogPanel::LogPanel()
    {
        m_editorSink = std::make_unique<Debug::EditorLogSink>();
        m_editorSinkObserver = m_editorSink.get();
        Debug::LogSystem::Instance().AddSink(std::move(m_editorSink));
    }

    LogPanel::~LogPanel() {}

    void LogPanel::Initialize(EditorContext* context)
    {
        _context = context;
    }

    void LogPanel::OnImGuiRender()
    {
        ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(GetName().c_str(), &_isActive))
        {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Clear"))
        {
            m_messages.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-Scroll", &m_autoScroll);

        ImGui::Separator();

        auto newMsgs = m_editorSinkObserver->FetchMessages();
        if (!newMsgs.empty())
        {
            m_messages.insert(m_messages.end(), newMsgs.begin(), newMsgs.end());
        }

        ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        ImGuiListClipper clipper;
        clipper.Begin(m_messages.size());
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const auto& msgPair = m_messages[i];

                // 默认颜色（白色）
                ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

                switch (msgPair.first)
                {
                case Debug::LogLevel::Info:
                    color = ImVec4(0.4f, 0.9f, 0.4f, 1.0f);
                    break;
                case Debug::LogLevel::Warn:
                    color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                    break;
                case Debug::LogLevel::Error:
                case Debug::LogLevel::Fatal:
                    color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    break;
                case Debug::LogLevel::Debug:
                    color = ImVec4(0.6f, 0.6f, 0.8f, 1.0f);
                    break;
                default:
                    break;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(msgPair.second.c_str());
                ImGui::PopStyleColor();
            }
        }
        clipper.End();

        // ---------------------------------------------------------
        // 4. 自动滚动逻辑
        // ---------------------------------------------------------
        // 只有当有新消息且开启了 Auto-Scroll 时，才滚到底部
        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() && !newMsgs.empty())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }
} // namespace ChikaEngine::Editor
