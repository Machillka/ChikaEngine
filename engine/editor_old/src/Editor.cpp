#include "Editor.h"
#include "ChikaEngine/gameobject/Camera.h"
#include "ChikaEngine/renderer.h"
#include "ChikaEngine/window/window_system.h"
#include "include/ChikaEngine/ImguiAdapter.h"
#include "include/ChikaEngine/ImguiInspectorPanel.h"
#include "include/ChikaEngine/ImguiContentBrowserPanel.h"
#include "include/ChikaEngine/ImguiLogPanel.h"
#include "include/ChikaEngine/ImguiViewPanel.h"
#include <memory>
#include <utility>

namespace ChikaEngine::Editor
{
    bool Editor::Init(Platform::IWindow* window)
    {
        _window = window;
        _editorUI = std::make_unique<ChikaEngine::Editor::ImGuiAdapter>(window->GetNativeHandle());
        _editorUI->Init(window->GetNativeHandle());
        _isInitalized = true;

        auto logPanel = std::make_unique<ChikaEngine::Editor::ImguiLogPanel>();
        _viewTarget = Render::Renderer::CreateRenderTarget(600, 900);
        _viewCamera = std::make_unique<Framework::Camera>("View Camera");
        auto viewPanel = std::make_unique<ChikaEngine::Editor::ImguiViewPanel>(_viewTarget, _viewCamera.get());
        auto inspector = std::make_unique<ChikaEngine::Editor::ImguiInspectorPanel>();
        auto browser = std::make_unique<ChikaEngine::Editor::ImguiContentBrowserPanel>();

        this->RegisterPanel(std::move(viewPanel)).RegisterPanel(std::move(logPanel)).RegisterPanel(std::move(inspector)).RegisterPanel(std::move(browser));

        return true;
    }

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
    void Editor::SetActiveScene(Framework::Scene* scene)
    {
        if (_editorUI)
        {
            _editorUI->GetContext().activeScene = scene; // 假设你的Adapter暴露了获取Context引用的方法
        }
    }
} // namespace ChikaEngine::Editor