#pragma once
#include "CommandManager.h"
#include "IEditorPanel.h"
#include "IEditorUI.h"

#include <memory>
#include <vector>

namespace ChikaEngine::Editor
{
    class Editor
    {
      public:
        bool Init(std::unique_ptr<IEditorUI> uiImpl, void* nativeWindow);
        void RegisterPanel(std::unique_ptr<IEditorPanel> panel);
        void Tick(); // Tick every frame
        void Shutdown();

      private:
        std::unique_ptr<IEditorUI> _editorUI;
        std::vector<std::unique_ptr<IEditorPanel>> _editorPanels;
        bool _isInitalized = false;
        CommandManager _cmdManager;
    };
} // namespace ChikaEngine::Editor