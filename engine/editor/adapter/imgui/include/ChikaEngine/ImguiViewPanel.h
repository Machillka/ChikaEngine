#pragma once

#include "ChikaEngine/RHI/RHIResources.h"
#include "ChikaEngine/gameobject/Camera.h"
#include "IEditorPanel.h"
#include "imgui.h"
namespace ChikaEngine::Editor
{
    class ImguiViewPanel : public IEditorPanel
    {
      public:
        explicit ImguiViewPanel(ChikaEngine::Render::IRHIRenderTarget* target, Framework::Camera* camera);
        ImguiViewPanel(int width, int height);
        ~ImguiViewPanel() = default;
        const char* Name() const override
        {
            return "Scene View";
        }

        void OnRender(UIContext& ctx) override;
        Render::IRHIRenderTarget* RenderTarget()
        {
            return _target;
        };

      private:
        void HandleMouseInteraction(const ImVec2& imagePos, const ImVec2& imageSize);
        void DrawOverlayControls();
        void HandleKeyboardInteraction(UIContext& ctx);
        void HandleRayCast(UIContext& ctx);

      private:
        ChikaEngine::Render::IRHIRenderTarget* _target = nullptr;
        Framework::Camera* _camera = nullptr; // 用于渲染view的摄像机
        bool _enableCameraControl = true;
        bool _rightMouseDown = false;
        bool _middleMouseDown = false;
        ImVec2 _lastMousePos = ImVec2(0.0f, 0.0f); // 缩放与平移灵敏度
        float _orbitSensitivity = 0.25f;           // 旋转灵敏度（度/像素）
        float _panSensitivity = 0.005f;            // 平移灵敏度（世界单位/像素）
        float _zoomSensitivity = 0.1f;             // 缩放灵敏度（比例）
        bool _showGizmos = true;                   // 面板内显示选项
        bool _fitToPanel = true;                   // 是否自动按面板大小显示纹理
    };
} // namespace ChikaEngine::Editor