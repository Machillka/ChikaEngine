#include "ChikaEngine/debug/Gizmo.hpp"

namespace ChikaEngine::Debug
{
    void Gizmo::DrawWireBox(const Math::Vector3& center, const Math::Vector3& halfExtents, const Math::Quaternion& rotation, const Math::Vector4& color)
    {
        // 局部坐标系的 8 个顶点
        Math::Vector3 corners[8] = { Math::Vector3(-halfExtents.x, -halfExtents.y, -halfExtents.z), Math::Vector3(halfExtents.x, -halfExtents.y, -halfExtents.z), Math::Vector3(halfExtents.x, halfExtents.y, -halfExtents.z), Math::Vector3(-halfExtents.x, halfExtents.y, -halfExtents.z),
                                     Math::Vector3(-halfExtents.x, -halfExtents.y, halfExtents.z),  Math::Vector3(halfExtents.x, -halfExtents.y, halfExtents.z),  Math::Vector3(halfExtents.x, halfExtents.y, halfExtents.z),  Math::Vector3(-halfExtents.x, halfExtents.y, halfExtents.z) };

        // 旋转并平移到世界坐标
        for (int i = 0; i < 8; ++i)
        {
            corners[i] = center + rotation.Rotate(corners[i]);
        }

        // 绘制底部 4 条边
        DrawLine(corners[0], corners[1], color);
        DrawLine(corners[1], corners[2], color);
        DrawLine(corners[2], corners[3], color);
        DrawLine(corners[3], corners[0], color);

        // 绘制顶部 4 条边
        DrawLine(corners[4], corners[5], color);
        DrawLine(corners[5], corners[6], color);
        DrawLine(corners[6], corners[7], color);
        DrawLine(corners[7], corners[4], color);

        // 绘制侧面 4 条连接边
        DrawLine(corners[0], corners[4], color);
        DrawLine(corners[1], corners[5], color);
        DrawLine(corners[2], corners[6], color);
        DrawLine(corners[3], corners[7], color);
    }
} // namespace ChikaEngine::Debug