#include <vector>
#include <cmath>
#include "ChikaEngine/Gizmo.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/math/vector4.h"

namespace ChikaEngine::Render
{
    void Gizmo::Clear()
    {
        _vertices.clear();
    }

    void Gizmo::DrawLine(const Math::Vector3& start, const Math::Vector3& end, const Math::Vector4& color)
    {
        if (color.x < 0.0f || color.x > 1.0f || color.y < 0.0f || color.y > 1.0f || color.z < 0.0f || color.z > 1.0f || color.w < 0.0f || color.w > 1.0f)
        {
            LOG_WARN("Gizmo", "DrawLine called with out-of-range color values");
            return;
        }

        if (_vertices.size() >= 65536) // ~32k line segments max per frame
        {
            LOG_WARN("Gizmo", "DrawLine: Vertex buffer getting too large ({}), skipping new lines", _vertices.size());
            return;
        }

        _vertices.push_back({start, color});
        _vertices.push_back({end, color});
    }

    void Gizmo::DrawLine(const Math::Vector3& start, const Math::Vector3& end, const Math::Vector4& color, const Math::Mat4& transform)
    {
        Math::Vector4 s = transform * Math::Vector4(start.x, start.y, start.z, 1.0f);
        Math::Vector4 e = transform * Math::Vector4(end.x, end.y, end.z, 1.0f);

        // 执行透视除法，处理齐次坐标
        float invWs = (s.w != 0.0f) ? (1.0f / s.w) : 1.0f;
        float invWe = (e.w != 0.0f) ? (1.0f / e.w) : 1.0f;

        Math::Vector3 startPos(s.x * invWs, s.y * invWs, s.z * invWs);
        Math::Vector3 endPos(e.x * invWe, e.y * invWe, e.z * invWe);

        DrawLine(startPos, endPos, color);
    }

    void Gizmo::DrawWireBox(const Math::Vector3& center, const Math::Vector3& halfExtents, const Math::Mat4& transform, const Math::Vector4& color)
    {
        if (halfExtents.x < 0.0f || halfExtents.y < 0.0f || halfExtents.z < 0.0f)
        {
            LOG_WARN("Gizmo", "DrawWireBox: invalid halfExtents (negative values)");
            return;
        }

        Math::Vector3 v[8] = {Math::Vector3(-halfExtents.x, -halfExtents.y, -halfExtents.z), //
                              Math::Vector3(halfExtents.x, -halfExtents.y, -halfExtents.z),
                              Math::Vector3(halfExtents.x, halfExtents.y, -halfExtents.z),
                              Math::Vector3(-halfExtents.x, halfExtents.y, -halfExtents.z),
                              Math::Vector3(-halfExtents.x, -halfExtents.y, halfExtents.z),
                              Math::Vector3(halfExtents.x, -halfExtents.y, halfExtents.z),
                              Math::Vector3(halfExtents.x, halfExtents.y, halfExtents.z),
                              Math::Vector3(-halfExtents.x, halfExtents.y, halfExtents.z)};

        // 加上 center 偏移
        for (int i = 0; i < 8; ++i)
        {
            v[i] += center;
        }

        // 绘制 12 条边 底面 4 条，顶面 4 条，竖的 4 条
        for (int i = 0; i < 4; ++i)
        {
            DrawLine(v[i], v[(i + 1) % 4], color, transform);         // 底面
            DrawLine(v[i + 4], v[(i + 1) % 4 + 4], color, transform); // 顶面
            DrawLine(v[i], v[i + 4], color, transform);               // 竖棱
        }
    }

    void Gizmo::DrawTransformAxis(const Math::Vector3& worldPos, const Math::Mat4& globalMatrix, float axisLength)
    {
        if (axisLength <= 0.0f)
        {
            LOG_WARN("Gizmo", "DrawTransformAxis: invalid axisLength (must be positive)");
            return;
        }

        Math::Vector4 right = globalMatrix * Math::Vector4(axisLength, 0, 0, 0);
        Math::Vector4 up = globalMatrix * Math::Vector4(0, axisLength, 0, 0);
        Math::Vector4 forward = globalMatrix * Math::Vector4(0, 0, axisLength, 0);

        Math::Vector3 rightAxis(right.x, right.y, right.z);
        Math::Vector3 upAxis(up.x, up.y, up.z);
        Math::Vector3 forwardAxis(forward.x, forward.y, forward.z);

        // X轴：红色 (1, 0, 0)
        DrawLine(worldPos, worldPos + rightAxis, Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f));
        // Y轴：绿色 (0, 1, 0)
        DrawLine(worldPos, worldPos + upAxis, Math::Vector4(0.0f, 1.0f, 0.0f, 1.0f));
        // Z轴：蓝色 (0, 0, 1)
        DrawLine(worldPos, worldPos + forwardAxis, Math::Vector4(0.0f, 0.0f, 1.0f, 1.0f));
    }

    void Gizmo::DrawWireSphere(const Math::Vector3& center, float radius, const Math::Vector4& color, int segments)
    {
        if (radius <= 0.0f || segments < 3)
        {
            LOG_WARN("Gizmo", "DrawWireSphere: invalid radius or segments");
            return;
        }

        segments = (segments > 128) ? 128 : segments;

        const float PI = 3.14159265359f;
        const float TWO_PI = 2.0f * PI;
        const float angleStep = TWO_PI / static_cast<float>(segments);

        // 绘制三个垂直的圆（XY、XZ、YZ 平面）
        // XY 平面圆
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = angleStep * i;
            float angle2 = angleStep * ((i + 1) % segments);

            Math::Vector3 p1(center.x + radius * std::cos(angle1), center.y + radius * std::sin(angle1), center.z);
            Math::Vector3 p2(center.x + radius * std::cos(angle2), center.y + radius * std::sin(angle2), center.z);
            DrawLine(p1, p2, color);
        }

        // XZ 平面圆
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = angleStep * i;
            float angle2 = angleStep * ((i + 1) % segments);

            Math::Vector3 p1(center.x + radius * std::cos(angle1), center.y, center.z + radius * std::sin(angle1));
            Math::Vector3 p2(center.x + radius * std::cos(angle2), center.y, center.z + radius * std::sin(angle2));
            DrawLine(p1, p2, color);
        }

        // YZ 平面圆
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = angleStep * i;
            float angle2 = angleStep * ((i + 1) % segments);

            Math::Vector3 p1(center.x, center.y + radius * std::cos(angle1), center.z + radius * std::sin(angle1));
            Math::Vector3 p2(center.x, center.y + radius * std::cos(angle2), center.z + radius * std::sin(angle2));
            DrawLine(p1, p2, color);
        }
    }
} // namespace ChikaEngine::Render