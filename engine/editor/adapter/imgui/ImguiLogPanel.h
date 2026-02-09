#pragma once

#include "debug/editor_sink.h"
#include "editor/include/IEditorPanel.h"

#include <string>
#include <vector>

namespace ChikaEngine::Editor
{
    // 用于渲染 Log 日志 使用 Imgui
    class ImguiLogPanel : public IEditorPanel
    {
      public:
        ImguiLogPanel();
        ~ImguiLogPanel() = default;
        const char* Name() const override
        {
            return "Log Panel";
        }
        void OnRender(UIContext& ctx) override;
        void SetEditorSink(ChikaEngine::Debug::EditorLogSink* sink);
        void Clear();
        void FlushFromSink();

      private:
        // 用于交换数据
        void ApplyMessages(const std::vector<ChikaEngine::Debug::MessagePair>& msgs);
        ChikaEngine::Debug::EditorLogSink* _editorSink = nullptr;
        // record log message for inside buff
        // TODO: 设置最大长度
        std::vector<std::string> _lines;

        std::string _filterText;
        bool _autoScroll = true;

        // 显示级别过滤
        bool _showDebug = true;
        bool _showInfo = true;
        bool _showWarn = true;
        bool _showError = true;
        bool _showFatal = true;
    };
} // namespace ChikaEngine::Editor