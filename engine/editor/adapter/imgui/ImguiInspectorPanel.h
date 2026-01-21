#pragma once

#include "editor/include/IEditorPanel.h"

namespace ChikaEngine::Editor
{
    class ImguiInspectorPanel : public IEditorPanel
    {
      public:
        ImguiInspectorPanel() = default;
        ~ImguiInspectorPanel() = default;
        const char* Name() const override
        {
            return "Inspector";
        }
        void OnRender(UIContext& ctx) override;
    };
} // namespace ChikaEngine::Editor
