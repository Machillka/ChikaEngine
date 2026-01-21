#include "ImguiViewPanel.h"

#include "IEditorPanel.h"
#include "debug/log_macros.h"
#include "imgui.h"
#include "render/camera.h"
#include "render/renderer.h"
#include "render/rhi/RHIResources.h"
#include "render/rhi/opengl/GLTexture2D.h"

#include <memory>

namespace ChikaEngine::Editor
{
    ImguiViewPanel::ImguiViewPanel(Render::IRHIRenderTarget* target)
    {
        _target = target;
        _camera = std::make_unique<ChikaEngine::Render::Camera>();
    }

    ImguiViewPanel::ImguiViewPanel(ChikaEngine::Render::IRHIRenderTarget* target, ChikaEngine::Render::Camera* camera) : _target(target), _camera(camera) {}

    void ImguiViewPanel::OnRender(UIContext& ctx)
    {
        ImGui::Begin(Name());
        DrawOverlayControls(); // 绘制顶部工具栏

        if (!_target)
        {
            LOG_WARN("view panel", "No render target assigned.");
            ImGui::TextDisabled("No render target assigned.");
            ImGui::End();
            return;
        }

        Render::IRHITexture2D* colorTex = _target->GetColorTexture();
        if (!colorTex)
        {
            LOG_WARN("view panel", "Render target has no color texture.");
            ImGui::TextDisabled("Render target has no color texture.");
            ImGui::End();
            return;
        }

        ImTextureID texId = colorTex->Handle();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 imageSize = avail;
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 imagePos = cursorPos;

        if (texId == 0)
        {
            ImGui::TextDisabled("Texture handle not available.");
            ImGui::TextDisabled("Expose native handle via IRHITexture2D to display.");
        }
        else
        {
            ImGui::Image(texId, imageSize, ImVec2(0, 1), ImVec2(1, 0));
        }

        // 交互（旋转/平移/缩放）
        HandleMouseInteraction(imagePos, imageSize);
        HandleKeyboardInteraction(ctx);
        ImGui::End();
    }

    void ImguiViewPanel::HandleMouseInteraction(const ImVec2& imagePos, const ImVec2& imageSize)
    {
        if (!_enableCameraControl || !_camera)
            return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = io.MousePos;

        bool isMouseInside = (mousePos.x >= imagePos.x && mousePos.x <= imagePos.x + imageSize.x && mousePos.y >= imagePos.y && mousePos.y <= imagePos.y + imageSize.y);

        bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        bool middleDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
        bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

        // 记录开始位置（ 拖拽检测
        if (isMouseInside && rightDown && !_rightMouseDown)
        {
            _rightMouseDown = true;
            _lastMousePos = mousePos;
        }

        if (!rightDown && _rightMouseDown)
        {
            _rightMouseDown = false;
        }
        if (isMouseInside && middleDown && !_middleMouseDown)
        {
            _middleMouseDown = true;
            _lastMousePos = mousePos;
        }
        if (!middleDown && _middleMouseDown)
        {
            _middleMouseDown = false;
        }

        // 处理旋转
        if (_rightMouseDown && isMouseInside)
        {
            float dx = mousePos.x - _lastMousePos.x;
            float dy = mousePos.y - _lastMousePos.y;
            _lastMousePos = mousePos;
            dx *= _orbitSensitivity;
            dy *= _orbitSensitivity;

            _camera->ProcessMouseMovement(static_cast<float>(dx), static_cast<float>(dy));
        }

        if ((_middleMouseDown && isMouseInside) || (isMouseInside && io.KeyAlt && leftDown))
        {
            float dx = mousePos.x - _lastMousePos.x;
            float dy = mousePos.y - _lastMousePos.y;
            _lastMousePos = mousePos;

            float moveX = -dx * _panSensitivity;
            float moveY = dy * _panSensitivity;
            _camera->transform->Translate(moveX, moveY, 0);
        }
    }
    void ImguiViewPanel::HandleKeyboardInteraction(UIContext& ctx)
    {
        const float moveSpeed = 5.0f;

        ChikaEngine::Math::Vector3 move{};

        if (!_enableCameraControl || !_camera)
            return; // 只有 Scene View 聚焦时才响应键盘
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            return;
        // float dt = ctx.DeltaTime();
        ImGuiIO& io = ImGui::GetIO();
        float dx = 0;
        float dy = 0;
        if (ImGui::IsKeyDown(ImGuiKey_W))
            move += _camera->Front();
        if (ImGui::IsKeyDown(ImGuiKey_S))
            move -= _camera->Front();
        if (ImGui::IsKeyDown(ImGuiKey_A))
            move += _camera->Front().Cross(ChikaEngine::Math::Vector3::up).Normalized();
        if (ImGui::IsKeyDown(ImGuiKey_D))
            move -= _camera->Front().Cross(ChikaEngine::Math::Vector3::up).Normalized();

        if (move.Length() > 0.0f)
            _camera->transform->Translate(move.Normalized() * moveSpeed * ctx.deltaTime);
    }
    void ImguiViewPanel::DrawOverlayControls()
    {
        if (ImGui::Button("Reset Camera"))
        {
            // TODO: 添加一个重置相机的方法
        }

        ImGui::SameLine();
        ImGui::Checkbox("Gizmos", &_showGizmos);
        ImGui::SameLine();
        ImGui::Checkbox("Fit", &_fitToPanel);
        ImGui::SameLine();
        ImGui::Checkbox("Cam Control", &_enableCameraControl);
    }

} // namespace ChikaEngine::Editor