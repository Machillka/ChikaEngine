#pragma once

#include "ChikaEngine/math/mat4.h"
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

        /** @brief 绘制具有任意旋转的线框 Box。 */
        static void DrawWireBox(const Math::Vector3& center, const Math::Vector3& halfExtents, const Math::Quaternion& rotation, const Math::Vector4& color = { 0.0f, 1.0f, 0.0f, 1.0f });

        /** @brief 绘制世界空间轴对齐包围盒。 */
        static void DrawWireAABB(const Math::Vector3& center, const Math::Vector3& halfExtents, const Math::Vector4& color = { 0.0f, 1.0f, 0.0f, 1.0f });

        /** @brief 从 Vulkan 0..1 深度范围的 ViewProjection 绘制世界空间视锥线框。 */
        static void DrawFrustum(const Math::Mat4& viewProjection, const Math::Vector4& color = { 0.0f, 0.8f, 1.0f, 1.0f });

        static const std::vector<GizmoLine>& GetLines()
        {
            return m_lines;
        }
        /** @brief 返回当前帧累计的调试线段数量，供诊断和测试读取。 */
        static size_t GetLineCount()
        {
            return m_lines.size();
        }
        static void Clear()
        {
            m_lines.clear();
        }

      private:
        static inline std::vector<GizmoLine> m_lines;
    };
} // namespace ChikaEngine::Debug
