#pragma once

#include "IEditorPanel.h"
#include "IEditorUI.h"

#include <vector>

namespace ChikaEngine::Editor
{

    class ImGuiAdapter : public IEditorUI
    {
      public:
        explicit ImGuiAdapter(void* nativeWindow = nullptr);
        ~ImGuiAdapter();
        bool Init(void* nativeWindow) override;
        void NewFrame() override;
        void RegisterPanel(IEditorPanel* panel) override;
        void UnregisterPanel(IEditorPanel* panel) override;
        void RenderAllPanels() override;
        void Render() override;
        void SwapBuffers() override;
        void Shutdown() override;
        void SaveLayout(const std::string& path) override;
        void LoadLayout(const std::string& path) override;
        void UpdateContext() override;

      private:
        void* _window;
        std::vector<IEditorPanel*> _panels;
        UIContext _ctx;
    };

} // namespace ChikaEngine::Editor