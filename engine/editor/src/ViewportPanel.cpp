#include "ViewportPanel.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/Camera.hpp"
#include "ChikaEngine/debug/Gizmo.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/IRHIDevice.hpp" // 用于获取 ImGui Texture Handle
#include "EditorGizmoRenderer.hpp"
#include <imgui.h>

namespace ChikaEngine::Editor
{
    struct Ray
    {
        Math::Vector3 origin;
        Math::Vector3 direction;
    };

    /*!
     * @brief 辅助函数：将屏幕坐标转换为世界空间射线
     */
    Ray CalculateRayFromScreen(const ImVec2& mousePos, const ImVec2& windowPos, const ImVec2& windowSize, Render::Renderer* renderer)
    {
        Ray ray;
        ray.origin = Math::Vector3(0, 0, 0);
        ray.direction = Math::Vector3(0, 0, -1);

        auto* camera = renderer->GetActiveCamera();
        if (!camera)
            return ray;

        // 1. 获取屏幕 UV (0.0 -> 1.0)
        float u = (mousePos.x - windowPos.x) / windowSize.x;
        float v = (mousePos.y - windowPos.y) / windowSize.y;

        // 2. 映射到 NDC 坐标 (-1.0 -> 1.0)
        float x = u * 2.0f - 1.0f;

        float y = v * 2.0f - 1.0f;

        Math::Vector4 ndcFar(x, y, 1.0f, 1.0f);

        // 4. 计算 ViewProjection 的整体逆矩阵
        Math::Mat4 vp = camera->GetProjectionMatrix() * camera->GetViewMatrix();
        Math::Mat4 invVP = vp.Inverse();

        // 5. 将远平面点转换到世界空间
        Math::Vector4 worldFar = invVP * ndcFar;

        // 必须进行透视除法
        if (worldFar.w != 0.0f)
        {
            worldFar.x /= worldFar.w;
            worldFar.y /= worldFar.w;
            worldFar.z /= worldFar.w;
        }

        // 6. 射线方向 = (远平面世界点 - 摄像机位置) 的归一化向量
        ray.origin = camera->GetPosition();
        ray.direction = (Math::Vector3(worldFar.x, worldFar.y, worldFar.z) - ray.origin).Normalized();

        return ray;
    }

    void ViewportPanel::Initialize(EditorContext* context)
    {
        _context = context;
    }

    void ViewportPanel::Tick(float deltaTime)
    {
        // 如果不在漫游状态，直接跳过
        if (!_isRoaming)
            return;

        auto* camera = _context->renderer->GetActiveCamera();
        if (!camera)
            return;

        float moveSpeed = 5.0f * deltaTime;

        if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
            moveSpeed *= 2.5f;

        float rotSpeed = 0.1f;

        ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
        Math::Vector3 camRot = camera->GetRotation();
        camRot.y -= mouseDelta.x * rotSpeed;
        camRot.x -= mouseDelta.y * rotSpeed;

        if (camRot.x > 89.0f)
            camRot.x = 89.0f;
        if (camRot.x < -89.0f)
            camRot.x = -89.0f;

        camera->SetRotation(camRot);

        Math::Vector3 pos = camera->GetPosition();
        Math::Vector3 forward = camera->GetForward();
        Math::Vector3 right = Math::Vector3::Cross(forward, Math::Vector3(0, 1, 0)).Normalized();

        if (ImGui::IsKeyDown(ImGuiKey_W))
            pos += forward * moveSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_S))
            pos -= forward * moveSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_A))
            pos -= right * moveSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_D))
            pos += right * moveSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_Q))
            pos.y -= moveSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_E))
            pos.y += moveSpeed;

        camera->SetPosition(pos);
    }

    void ViewportPanel::OnImGuiRender()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin(GetName().c_str(), &_isActive, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        _context->isViewportHovered = ImGui::IsWindowHovered();
        _context->isViewportFocused = ImGui::IsWindowFocused();

        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        ImVec2 windowPos = ImGui::GetCursorScreenPos();

        if (windowSize.x > 0 && windowSize.y > 0)
        {
            if (windowSize.x != _context->renderer->GetViewportWidth() || windowSize.y != _context->renderer->GetViewportHeight())
            {
                _context->renderer->OnViewResize((uint32_t)windowSize.x, (uint32_t)windowSize.y);
            }

            Render::TextureHandle offscreenTex = _context->renderer->GetOffscreenTexture();
            void* imguiTexID = _context->renderer->GetRHIHandle()->GetImGuiTextureHandle(offscreenTex);

            if (imguiTexID)
            {
                ImGui::Image(imguiTexID, windowSize);
            }
        }

        if (_context->isViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            _isRoaming = true;
            ImGui::SetWindowFocus();
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            _isRoaming = false;
        }

        if (_context->isViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            Ray ray = CalculateRayFromScreen(mousePos, windowPos, windowSize, _context->renderer);

            if (_context->activeScene)
            {
                auto* physics = _context->activeScene->GetPhysicsSubsystem();
                if (physics)
                {
                    Physics::RaycastHit hit;

                    if (physics->Raycast(ray.origin, ray.direction, 1000.0f, hit))
                    {
                        _context->selectedGameObject = hit.gameObjectId;
                    }
                    else
                    {
                        _context->selectedGameObject = 0;
                    }
                }
            }
        }

        if (_context->activeScene && _context->renderer->GetActiveCamera())
        {
            Debug::Gizmo::Clear();

            const auto& gameObjects = _context->activeScene->GetAllGameobjects();
            for (const auto& go : gameObjects)
            {
                if (_context->selectedGameObject == go->GetID())
                {
                    for (auto& comp : go->GetAllComponents())
                    {
                        comp->OnGizmo();
                    }
                }
            }

            auto* drawList = ImGui::GetWindowDrawList();
            EditorGizmoRenderer::Render(_context->renderer->GetActiveCamera(), drawList, windowPos, windowSize);
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }
} // namespace ChikaEngine::Editor