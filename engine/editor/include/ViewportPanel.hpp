#pragma once

#include "IEditorPanel.hpp"
#include <string>
namespace ChikaEngine::Editor
{
    // ~ViewportPanel() = default;
    // void Initialize(EditorContext* context);
    // void Tick(float deltaTime);
    // void OnImGuiRender();
    // const std::string& GetName() const;

    class ViewportPanel : public IEditorPanel
    {
      public:
        ~ViewportPanel() = default;
        void Initialize(EditorContext* context) override;
        void Tick(float deltaTime) override;
        void OnImGuiRender() override;
        const std::string& GetName() const override
        {
            static const std::string name = "Viewport Panel";
            return name;
        };

      private:
    };
} // namespace ChikaEngine::Editor