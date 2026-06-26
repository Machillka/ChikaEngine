#include "ChikaEngine/RenderVisibility.hpp"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"

#include <cmath>

namespace ChikaEngine::Render
{
    namespace
    {
        /**
         * @brief 归一化平面，使 AABB 投影半径可以直接与有符号距离比较。
         */
        FrustumPlane NormalizePlane(float x, float y, float z, float distance)
        {
            const float length = std::sqrt(x * x + y * y + z * z);
            if (length <= 0.000001f)
                return {};
            return {
                .normal = { x / length, y / length, z / length },
                .distance = distance / length,
            };
        }
    } // namespace

    ViewFrustum ViewFrustum::FromViewProjection(const Math::Mat4& matrix)
    {
        ViewFrustum frustum;
        frustum.m_planes = {
            NormalizePlane(matrix(3, 0) + matrix(0, 0), matrix(3, 1) + matrix(0, 1), matrix(3, 2) + matrix(0, 2), matrix(3, 3) + matrix(0, 3)),
            NormalizePlane(matrix(3, 0) - matrix(0, 0), matrix(3, 1) - matrix(0, 1), matrix(3, 2) - matrix(0, 2), matrix(3, 3) - matrix(0, 3)),
            NormalizePlane(matrix(3, 0) + matrix(1, 0), matrix(3, 1) + matrix(1, 1), matrix(3, 2) + matrix(1, 2), matrix(3, 3) + matrix(1, 3)),
            NormalizePlane(matrix(3, 0) - matrix(1, 0), matrix(3, 1) - matrix(1, 1), matrix(3, 2) - matrix(1, 2), matrix(3, 3) - matrix(1, 3)),
            NormalizePlane(matrix(2, 0), matrix(2, 1), matrix(2, 2), matrix(2, 3)),
            NormalizePlane(matrix(3, 0) - matrix(2, 0), matrix(3, 1) - matrix(2, 1), matrix(3, 2) - matrix(2, 2), matrix(3, 3) - matrix(2, 3)),
        };
        return frustum;
    }

    bool ViewFrustum::Intersects(const RenderBounds& bounds) const
    {
        // 无效 Bounds 采用可见降级，避免导入异常导致物体永久消失。
        if (!bounds.valid)
            return true;

        for (const FrustumPlane& plane : m_planes)
        {
            const float projectedRadius = std::abs(plane.normal.x) * bounds.extents.x + std::abs(plane.normal.y) * bounds.extents.y + std::abs(plane.normal.z) * bounds.extents.z;
            const float signedDistance = Math::Vector3::Dot(plane.normal, bounds.center) + plane.distance;
            if (signedDistance + projectedRadius < 0.0f)
                return false;
        }
        return true;
    }

    const std::array<FrustumPlane, 6>& ViewFrustum::GetPlanes() const
    {
        return m_planes;
    }

    bool IsRenderInstanceVisible(const RenderInstanceData& instance, const RenderView& view, const ViewFrustum& frustum, bool shadowCastersOnly)
    {
        const bool layerVisible = (instance.layerMask & view.layerMask) != 0;
        const bool passVisible = HasFlag(instance.flags, RenderObjectFlags::Visible) && (!shadowCastersOnly || HasFlag(instance.flags, RenderObjectFlags::CastShadow));
        return layerVisible && passVisible && frustum.Intersects(instance.bounds);
    }

    VisibilityResult BuildVisibility(const RenderWorldSnapshot& snapshot, const RenderView& view, bool shadowCastersOnly)
    {
        CHIKA_PROFILE_SCOPE("Renderer.Visibility");
        VisibilityResult result;
        result.visibleObjects.reserve(snapshot.objects.size());
        const ViewFrustum frustum = ViewFrustum::FromViewProjection(view.viewProjection);

        for (const RenderObjectSnapshot& object : snapshot.objects)
        {
            const RenderObjectProxy& proxy = object.proxy;
            ++result.testedObjectCount;
            const bool layerVisible = (proxy.layerMask & view.layerMask) != 0;
            const bool passVisible = HasFlag(proxy.flags, RenderObjectFlags::Visible) && (!shadowCastersOnly || HasFlag(proxy.flags, RenderObjectFlags::CastShadow));
            if (layerVisible && passVisible && frustum.Intersects(proxy.bounds))
            {
                result.visibleObjects.push_back(&object);
                ++result.visibleObjectCount;
            }
            else
                ++result.culledObjectCount;
        }
        return result;
    }

    VisibilityResult BuildVisibilitySerial(const RenderSceneView& scene, const RenderView& view, bool shadowCastersOnly)
    {
        CHIKA_PROFILE_SCOPE("Renderer.Visibility.Serial");
        VisibilityResult result;
        result.visibleObjects.reserve(scene.GetInstances().size());
        const ViewFrustum frustum = ViewFrustum::FromViewProjection(view.viewProjection);
        for (const RenderInstanceData& instance : scene.GetInstances())
        {
            ++result.testedObjectCount;
            if (IsRenderInstanceVisible(instance, view, frustum, shadowCastersOnly))
            {
                if (const RenderObjectSnapshot* object = scene.GetSourceObject(instance))
                {
                    result.visibleObjects.push_back(object);
                    ++result.visibleObjectCount;
                    continue;
                }
            }
            ++result.culledObjectCount;
        }
        return result;
    }
} // namespace ChikaEngine::Render
