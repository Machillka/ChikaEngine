#pragma once

#include "IEditorPanel.hpp"

namespace ChikaEngine::Framework
{
    class GameObject;
}

namespace ChikaEngine::Editor
{
    class SceneHierarchyPanel final : public IEditorPanel
    {
      public:
        void Initialize(EditorContext* context) override
        {
            _context = context;
        }
        void Tick(float deltaTime) override {}
        void OnImGuiRender() override;
        const std::string& GetName() const override
        {
            static const std::string name = "Scene";
            return name;
        }

      private:
        void DrawGameObjectNode(Framework::GameObject& gameObject);
    };
} // namespace ChikaEngine::Editor
