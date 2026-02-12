#include "Editor.h"
#include "ChikaEngine/gameobject/camera.h"
#include "ChikaEngine/renderer.h"
#include "ChikaEngine/window/window_system.h"
#include "include/ChikaEngine/ImguiAdapter.h"
#include "include/ChikaEngine/ImguiInspectorPanel.h"
#include "include/ChikaEngine/ImguiLogPanel.h"
#include "include/ChikaEngine/ImguiViewPanel.h"
#include <memory>
#include <utility>

namespace ChikaEngine::Editor
{
    Editor::Editor(Platform::IWindow* window)
    {
        _window = window;
        _editorUI = std::make_unique<ChikaEngine::Editor::ImGuiAdapter>(window->GetNativeHandle());
        _editorUI->Init(window->GetNativeHandle());
        _isInitalized = true;

        auto logPanel = std::make_unique<ChikaEngine::Editor::ImguiLogPanel>();
        _viewTarget = Render::Renderer::CreateRenderTarget(600, 900);
        _viewCamera = std::make_unique<Framework::Camera>();
        auto viewPanel = std::make_unique<ChikaEngine::Editor::ImguiViewPanel>(_viewTarget, _viewCamera.get());
        auto inspector = std::make_unique<ChikaEngine::Editor::ImguiInspectorPanel>();
        // Register panels: view center, log bottom, inspector right
        this->RegisterPanel(std::move(viewPanel)).RegisterPanel(std::move(logPanel)).RegisterPanel(std::move(inspector));
    }

    // bool Editor::Init(std::unique_ptr<IEditorUI> uiImpl, void* nativeWindow)
    // {
    //     if (!uiImpl)
    //         return false;
    //     _editorUI = std::move(uiImpl);
    //     if (!_editorUI->Init(nativeWindow))
    //         return false;
    //     _isInitalized = true;
    //     return true;
    // }

    Editor& Editor::RegisterPanel(std::unique_ptr<IEditorPanel> panel)
    {
        if (!panel)
            return *this;

        IEditorPanel* raw = panel.get();
        _editorUI->RegisterPanel(raw);
        _editorPanels.push_back(std::move(panel));

        return *this;
    }

    void Editor::Tick()
    {
        if (!_isInitalized)
            return;
        _editorUI->UpdateContext();
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