#pragma once
#include "ChikaEngine/RHI/RHIResources.h"
#include "ChikaEngine/gameobject/camera.h"
#include "ChikaEngine/render_device.h"
#include "CommandManager.h"
#include "IEditorPanel.h"
#include "IEditorUI.h"
namespace ChikaEngine::Platform
{
    class IWindow;
}

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
        Render::CameraData ViewCameraHandle()
        {
            return _viewCamera->ToRenderData();
        }

      private:
        Platform::IWindow* _window;
        Render::IRHIRenderTarget* _viewTarget;
        std::unique_ptr<Framework::Camera> _viewCamera;
        Editor& RegisterPanel(std::unique_ptr<IEditorPanel> panel);
        std::unique_ptr<IEditorUI> _editorUI;
        std::vector<std::unique_ptr<IEditorPanel>> _editorPanels;
        bool _isInitalized = false;
        CommandManager _cmdManager;
    };
} // namespace ChikaEngine::Editor