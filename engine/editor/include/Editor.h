#pragma once
#include "CommandManager.h"
#include "IEditorPanel.h"
#include "IEditorUI.h"
#include "render/camera.h"
#include "render/rhi/RHIResources.h"
#include "window/window_system.h"

#include <memory>
#include <vector>

namespace ChikaEngine::Editor
{
    class Editor
    {
      public:
        explicit Editor(Platform::IWindow* window);
        // bool Init(std::unique_ptr<IEditorUI> uiImpl, void* nativeWindow);
        void Tick(); // Tick every frame
        void Shutdown();
        void SetupPanel();
        Render::IRHIRenderTarget* ViewTargetHandle()
        {
            return _viewTarget;
        }
        Render::Camera* ViewCameraHandle()
        {
            return _viewCamera.get();
        }

      private:
        Platform::IWindow* _window;
        Render::IRHIRenderTarget* _viewTarget;
        std::unique_ptr<Render::Camera> _viewCamera;
        Editor& RegisterPanel(std::unique_ptr<IEditorPanel> panel);
        std::unique_ptr<IEditorUI> _editorUI;
        std::vector<std::unique_ptr<IEditorPanel>> _editorPanels;
        bool _isInitalized = false;
        CommandManager _cmdManager;
    };
} // namespace ChikaEngine::Editor