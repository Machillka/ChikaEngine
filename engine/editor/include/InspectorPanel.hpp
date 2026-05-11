#pragma once
#include "IEditorPanel.hpp"
#include <string>

namespace ChikaEngine::Editor
{
    class InspectorPanel : public IEditorPanel
    {
      public:
        ~InspectorPanel() = default;
        void Initialize(EditorContext* context) override
        {
            _context = context;
        }
        void Tick(float deltaTime) override {}
        void OnImGuiRender() override;
        const std::string& GetName() const override
        {
            static const std::string name = "Inspector";
            return name;
        }
    };
} // namespace ChikaEngine::Editor