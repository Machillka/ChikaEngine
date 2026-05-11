#pragma once

#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/math/vector4.h"
#include <vector>

namespace ChikaEngine::Debug
{
    struct GizmoLine
    {
        Math::Vector3 start;
        Math::Vector3 end;
        Math::Vector4 color;
    };

    // 全局数据收集器
    class Gizmo
    {
      public:
        // 绘制基础线段
        static void DrawLine(const Math::Vector3& start, const Math::Vector3& end, const Math::Vector4& color = { 0.0f, 1.0f, 0.0f, 1.0f })
        {
            m_lines.push_back({ start, end, color });
        }

        static void DrawWireBox(const Math::Vector3& center, const Math::Vector3& halfExtents, const Math::Quaternion& rotation, const Math::Vector4& color = { 0.0f, 1.0f, 0.0f, 1.0f });

        static const std::vector<GizmoLine>& GetLines()
        {
            return m_lines;
        }
        static void Clear()
        {
            m_lines.clear();
        }

      private:
        static inline std::vector<GizmoLine> m_lines;
    };
} // namespace ChikaEngine::Debug