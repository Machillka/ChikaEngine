#pragma once

#include "ChikaEngine/RenderSceneView.hpp"
#include "ChikaEngine/RenderWorld.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace ChikaEngine::Render
{
    /**
     * @brief 保存归一化视锥平面，平面内侧满足 dot(normal, point) + distance >= 0。
     */
    struct FrustumPlane
    {
        Math::Vector3 normal{};
        float distance = 0.0f;
    };

    /**
     * @brief 从 RenderView 构建的六平面视锥，用于 RenderWorld 空间剔除。
     */
    class ViewFrustum
    {
      public:
        /** @brief 从 Vulkan 0..1 深度范围的 ViewProjection Matrix 提取六个平面。 */
        static ViewFrustum FromViewProjection(const Math::Mat4& viewProjection);

        /** @brief 使用 World AABB 与视锥做保守相交测试。 */
        bool Intersects(const RenderBounds& bounds) const;
        /** @brief 暴露归一化平面给 GPU culling UBO，避免 CPU/GPU 使用两套提取逻辑。 */
        const std::array<FrustumPlane, 6>& GetPlanes() const;

      private:
        std::array<FrustumPlane, 6> m_planes;
    };

    /**
     * @brief 保存某个 View 的可见对象和剔除统计，后续 Queue 不再处理不可见代理。
     */
    struct VisibilityResult
    {
        std::vector<const RenderObjectSnapshot*> visibleObjects;
        uint32_t testedObjectCount = 0;
        uint32_t visibleObjectCount = 0;
        uint32_t culledObjectCount = 0;
    };

    /** @brief Applies the shared layer, flag and frustum contract to compact instance data. */
    bool IsRenderInstanceVisible(const RenderInstanceData& instance, const RenderView& view, const ViewFrustum& frustum, bool shadowCastersOnly = false);

    /**
     * @brief 按 View Layer、Visible Flag、可选 Shadow Flag 和 Frustum 生成可见对象集合。
     */
    VisibilityResult BuildVisibility(const RenderWorldSnapshot& snapshot, const RenderView& view, bool shadowCastersOnly = false);

    /** @brief Serial oracle over category-contiguous RenderSceneView data. */
    VisibilityResult BuildVisibilitySerial(const RenderSceneView& scene, const RenderView& view, bool shadowCastersOnly = false);
} // namespace ChikaEngine::Render
