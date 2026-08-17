#pragma once
#include "IEditorPanel.hpp"
#include <string>
#include <unordered_set>

namespace ChikaEngine::Framework
{
    class MeshRenderer;
}

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

      private:
        void DrawMeshRendererMaterialPanel(Framework::MeshRenderer& meshRenderer);

      private:
        std::string _materialEditError;
        std::unordered_set<uint32_t> _failedMaterialUploads;
    };
} // namespace ChikaEngine::Editor
