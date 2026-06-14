#pragma once

#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"

namespace ChikaEngine::Math
{
    /**
     * @brief 保存轴对齐包围盒与其外接球，作为 Asset、Resource 和 RenderWorld 的共同空间契约。
     *
     * center/extents 用于精确的 AABB 剔除，sphereRadius 为未来距离剔除和粗粒度层级结构保留。
     */
    struct Bounds
    {
        Vector3 center{};
        Vector3 extents{};
        float sphereRadius = 0.0f;
        bool valid = false;

        /** @brief 从最小点和最大点构造有效 Local Bounds。 */
        static Bounds FromMinMax(const Vector3& minimum, const Vector3& maximum);

        bool operator==(const Bounds&) const = default;
    };

    /**
     * @brief 将 Local AABB 保守转换为 World AABB。
     *
     * 通过 World Matrix 绝对值旋转缩放局部 extents，旋转后仍保持轴对齐且不会漏掉物体。
     */
    Bounds TransformBounds(const Bounds& localBounds, const Mat4& worldTransform);

    /** @brief 按中心等比例扩张 Bounds，用于暂未计算动态 Bounds 的蒙皮对象。 */
    Bounds ExpandBounds(const Bounds& bounds, float scale);
} // namespace ChikaEngine::Math
