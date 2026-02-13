#pragma once

#include "IEditorPanel.h"
#include "ChikaEngine/reflection/ReflectionData.h"

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
        void DrawPropertyWidget(void* instance, const Reflection::PropertyInfo& prop);
    };
} // namespace ChikaEngine::Editor
