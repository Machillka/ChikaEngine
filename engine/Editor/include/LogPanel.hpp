#pragma once

#include "ChikaEngine/debug/editor_sink.h"
#include "IEditorPanel.hpp"
#include <memory>
#include <vector>

namespace ChikaEngine::Editor
{
    class LogPanel : public IEditorPanel
    {
      public:
        LogPanel();
        ~LogPanel();

        void Initialize(EditorContext* context) override;
        void Tick(float deltaTime) override {}
        void OnImGuiRender() override;

        const std::string& GetName() const override
        {
            static const std::string name = "Log System";
            return name;
        }

      private:
        // 转移到 LogSystem 中
        std::unique_ptr<Debug::EditorLogSink> m_editorSink;
        // NOTE: 作为观察, 虽然可以直接使用 Shared ptr 但是效率是否会降低？？
        Debug::EditorLogSink* m_editorSinkObserver = nullptr;

        std::vector<Debug::MessagePair> m_messages;

        bool m_autoScroll = true;
    };
} // namespace ChikaEngine::Editor
