#include "ChikaEngine/debug/Gizmo.hpp"

#include <array>
#include <cmath>

namespace ChikaEngine::Debug
{
    namespace
    {
        constexpr std::array<std::array<size_t, 2>, 12> BoxEdges{
            std::array<size_t, 2>{ 0, 1 }, std::array<size_t, 2>{ 1, 2 }, std::array<size_t, 2>{ 2, 3 }, std::array<size_t, 2>{ 3, 0 }, std::array<size_t, 2>{ 4, 5 }, std::array<size_t, 2>{ 5, 6 }, std::array<size_t, 2>{ 6, 7 }, std::array<size_t, 2>{ 7, 4 }, std::array<size_t, 2>{ 0, 4 }, std::array<size_t, 2>{ 1, 5 }, std::array<size_t, 2>{ 2, 6 }, std::array<size_t, 2>{ 3, 7 },
        };

        /** @brief 将八个 Box Corner 按固定拓扑追加为十二条线段。 */
        void DrawBoxEdges(const std::array<Math::Vector3, 8>& corners, const Math::Vector4& color)
        {
            for (const auto& edge : BoxEdges)
                Gizmo::DrawLine(corners[edge[0]], corners[edge[1]], color);
        }
    } // namespace

    void Gizmo::DrawWireBox(const Math::Vector3& center, const Math::Vector3& halfExtents, const Math::Quaternion& rotation, const Math::Vector4& color)
    {
        std::array<Math::Vector3, 8> corners{
            Math::Vector3(-halfExtents.x, -halfExtents.y, -halfExtents.z), Math::Vector3(halfExtents.x, -halfExtents.y, -halfExtents.z), Math::Vector3(halfExtents.x, halfExtents.y, -halfExtents.z), Math::Vector3(-halfExtents.x, halfExtents.y, -halfExtents.z),
            Math::Vector3(-halfExtents.x, -halfExtents.y, halfExtents.z),  Math::Vector3(halfExtents.x, -halfExtents.y, halfExtents.z),  Math::Vector3(halfExtents.x, halfExtents.y, halfExtents.z),  Math::Vector3(-halfExtents.x, halfExtents.y, halfExtents.z),
        };

        for (Math::Vector3& corner : corners)
            corner = center + rotation.Rotate(corner);
        DrawBoxEdges(corners, color);
    }

    void Gizmo::DrawWireAABB(const Math::Vector3& center, const Math::Vector3& halfExtents, const Math::Vector4& color)
    {
        DrawWireBox(center, halfExtents, Math::Quaternion::Identity(), color);
    }

    void Gizmo::DrawFrustum(const Math::Mat4& viewProjection, const Math::Vector4& color)
    {
        constexpr std::array<Math::Vector4, 8> clipCorners{
            Math::Vector4(-1.0f, -1.0f, 0.0f, 1.0f), Math::Vector4(1.0f, -1.0f, 0.0f, 1.0f), Math::Vector4(1.0f, 1.0f, 0.0f, 1.0f), Math::Vector4(-1.0f, 1.0f, 0.0f, 1.0f), Math::Vector4(-1.0f, -1.0f, 1.0f, 1.0f), Math::Vector4(1.0f, -1.0f, 1.0f, 1.0f), Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f), Math::Vector4(-1.0f, 1.0f, 1.0f, 1.0f),
        };

        const Math::Mat4 inverseViewProjection = viewProjection.Inverse();
        std::array<Math::Vector3, 8> worldCorners;
        for (size_t index = 0; index < clipCorners.size(); ++index)
        {
            const Math::Vector4 corner = inverseViewProjection * clipCorners[index];
            if (std::abs(corner.w) <= 0.000001f)
                return;
            worldCorners[index] = { corner.x / corner.w, corner.y / corner.w, corner.z / corner.w };
        }
        DrawBoxEdges(worldCorners, color);
    }
} // namespace ChikaEngine::Debug
