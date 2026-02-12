
#include "include/ChikaEngine/ImguiViewPanel.h"
#include "ChikaEngine/gameobject/camera.h"
#include "imgui.h"

namespace ChikaEngine::Editor
{

    ImguiViewPanel::ImguiViewPanel(ChikaEngine::Render::IRHIRenderTarget* target, Framework::Camera* camera) : _target(target), _camera(camera) {}

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
        LOG_INFO("ViewPanel", "Texture handle = {}", texId);
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
        HandleKeyboardInteraction(ctx);
        HandleMouseInteraction(imagePos, imageSize);

        ImGui::End();
    }

    void ImguiViewPanel::HandleMouseInteraction(const ImVec2& imagePos, const ImVec2& imageSize)
    {
        if (!_enableCameraControl || !_camera)
            return;
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
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

            _camera->transform->ProcessLookDelta(dx, dy, true);
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
        const float dt = ctx.deltaTime;
        ChikaEngine::Math::Vector3 move{};

        if (!_enableCameraControl || !_camera)
            return;
        // 只有 Scene View 聚焦时才响应键盘
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            return;

        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsKeyDown(ImGuiKey_W))
            _camera->transform->TranslateLocal(0, 0, -moveSpeed * dt); // -Z = Forward
        if (ImGui::IsKeyDown(ImGuiKey_S))
            _camera->transform->TranslateLocal(0, 0, moveSpeed * dt);
        if (ImGui::IsKeyDown(ImGuiKey_A))
            _camera->transform->TranslateLocal(-moveSpeed * dt, 0, 0); // -X = Left
        if (ImGui::IsKeyDown(ImGuiKey_D))
            _camera->transform->TranslateLocal(moveSpeed * dt, 0, 0);
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