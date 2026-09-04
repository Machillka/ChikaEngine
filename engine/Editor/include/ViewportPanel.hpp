#pragma once
#include "IEditorPanel.hpp"
#include <string>

namespace ChikaEngine::Editor
{
    class ViewportPanel : public IEditorPanel
    {
      public:
        ~ViewportPanel() = default;
        void Initialize(EditorContext* context) override;
        void Tick(float deltaTime) override;
        void OnImGuiRender() override;
        const std::string& GetName() const override
        {
            static const std::string name = "Viewport";
            return name;
        }

      private:
        uint32_t _viewportWidth = 0;
        uint32_t _viewportHeight = 0;

        bool _isRoaming = false;
    };
} // namespace ChikaEngine::Editor