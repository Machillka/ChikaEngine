#include "EditorGizmoRenderer.hpp"
#include "ChikaEngine/debug/Gizmo.hpp"

namespace ChikaEngine::Editor
{
    bool EditorGizmoRenderer::WorldToScreen(const Math::Vector3& worldPos, const Math::Mat4& vpMatrix, const ImVec2& windowPos, const ImVec2& windowSize, ImVec2& outScreenPos)
    {
        Math::Vector4 clipPos = vpMatrix * Math::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f);

        if (clipPos.w < 0.01f)
            return false;

        Math::Vector3 ndc(clipPos.x / clipPos.w, clipPos.y / clipPos.w, clipPos.z / clipPos.w);

        outScreenPos.x = windowPos.x + (ndc.x + 1.0f) * 0.5f * windowSize.x;
        outScreenPos.y = windowPos.y + (ndc.y + 1.0f) * 0.5f * windowSize.y;

        return true;
    }

    void EditorGizmoRenderer::Render(Render::Camera* camera, ImDrawList* drawList, const ImVec2& windowPos, const ImVec2& windowSize)
    {
        if (!camera || !drawList)
            return;

        Math::Mat4 vp = camera->GetViewProjectionMatrix();
        const auto& lines = Debug::Gizmo::GetLines();

        for (const auto& line : lines)
        {
            ImVec2 p1, p2;
            bool front1 = WorldToScreen(line.start, vp, windowPos, windowSize, p1);
            bool front2 = WorldToScreen(line.end, vp, windowPos, windowSize, p2);

            if (front1 && front2)
            {
                ImU32 col = IM_COL32((int)(line.color.x * 255), (int)(line.color.y * 255), (int)(line.color.z * 255), (int)(line.color.w * 255));
                drawList->AddLine(p1, p2, col, 2.0f);
            }
        }
    }
} // namespace ChikaEngine::Editor