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
        explicit Editor() = default;
        bool Init(Platform::IWindow* window);
        void Tick(); // Tick every frame
        void Shutdown();
        void SetupPanel();
        Render::IRHIRenderTarget* ViewTargetHandle()
        {
            return _viewTarget;
        }
        Render::CameraData ViewCameraData()
        {
            return _viewCamera->ToRenderData();
        }

      private:
        Platform::IWindow* _window;
        Render::IRHIRenderTarget* _viewTarget;
        Editor& RegisterPanel(std::unique_ptr<IEditorPanel> panel);
        std::unique_ptr<IEditorUI> _editorUI;
        std::vector<std::unique_ptr<IEditorPanel>> _editorPanels;
        bool _isInitalized = false;
        CommandManager _cmdManager;

        std::unique_ptr<Framework::Camera> _viewCamera;
    };
} // namespace ChikaEngine::Editor