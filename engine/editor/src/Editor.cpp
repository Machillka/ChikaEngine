#include "editor/include/Editor.h"

#include "editor/include/IEditorPanel.h"

#include <utility>

namespace ChikaEngine::Editor
{

    bool Editor::Init(std::unique_ptr<IEditorUI> uiImpl, void* nativeWindow)
    {
        if (!uiImpl)
            return false;
        _editorUI = std::move(uiImpl);
        if (!_editorUI->Init(nativeWindow))
            return false;
        _isInitalized = true;
        return true;
    }

    void Editor::RegisterPanel(std::unique_ptr<IEditorPanel> panel)
    {
        if (!panel)
            return;
        IEditorPanel* raw = panel.get();
        _editorUI->RegisterPanel(raw);
        _editorPanels.push_back(std::move(panel));
    }

    void Editor::Tick()
    {
        if (!_isInitalized)
            return;
        _editorUI->NewFrame();
        _editorUI->RenderAllPanels();
        _editorUI->Render();
        _editorUI->SwapBuffers();
    }

    void Editor::Shutdown()
    {
        if (!_isInitalized)
            return;
        _editorUI->Shutdown();
        _isInitalized = false;
    }
} // namespace ChikaEngine::Editor