#include "ChikaEngine/RenderDebugVisualization.hpp"

#include "ChikaEngine/RenderVisibility.hpp"
#include "ChikaEngine/debug/Gizmo.hpp"

namespace ChikaEngine::Render
{
    namespace
    {
        constexpr Math::Vector4 VisibleBoundsColor{ 0.1f, 1.0f, 0.2f, 1.0f };
        constexpr Math::Vector4 CulledBoundsColor{ 1.0f, 0.15f, 0.1f, 1.0f };
        constexpr Math::Vector4 PrimaryViewFrustumColor{ 0.0f, 0.8f, 1.0f, 1.0f };
        constexpr Math::Vector4 SecondaryViewFrustumColor{ 0.8f, 0.2f, 1.0f, 1.0f };
        constexpr Math::Vector4 LightFrustumColor{ 1.0f, 0.75f, 0.0f, 1.0f };

        /** @brief 使用与 Visibility 阶段一致的 Flag、Layer 与 Frustum 规则判定调试颜色。 */
        bool IsVisibleInView(const RenderObjectProxy& proxy, const RenderView& view, const ViewFrustum& frustum)
        {
            return HasFlag(proxy.flags, RenderObjectFlags::Visible) && (proxy.layerMask & view.layerMask) != 0 && frustum.Intersects(proxy.bounds);
        }
    } // namespace

    void AppendRenderWorldDebugGizmos(const RenderWorldSnapshot& snapshot, const RenderDebugVisualizationOptions& options)
    {
        const RenderView* primaryView = snapshot.viewFamily.GetPrimaryView();
        if (options.drawAABBs)
        {
            const ViewFrustum primaryFrustum = primaryView ? ViewFrustum::FromViewProjection(primaryView->viewProjection) : ViewFrustum{};
            for (const RenderObjectSnapshot& object : snapshot.objects)
            {
                if (!object.proxy.bounds.valid)
                    continue;
                const bool visible = !primaryView || IsVisibleInView(object.proxy, *primaryView, primaryFrustum);
                Debug::Gizmo::DrawWireAABB(object.proxy.bounds.center, object.proxy.bounds.extents, visible ? VisibleBoundsColor : CulledBoundsColor);
            }
        }

        if (!options.drawFrustums)
            return;

        for (const RenderViewSnapshot& view : snapshot.viewFamily.views)
            Debug::Gizmo::DrawFrustum(view.view.viewProjection, view.view.primary ? PrimaryViewFrustumColor : SecondaryViewFrustumColor);
        for (const RenderLightSnapshot& light : snapshot.lights)
        {
            if (light.proxy.castsShadow)
                Debug::Gizmo::DrawFrustum(light.proxy.viewProjection, LightFrustumColor);
        }
    }
} // namespace ChikaEngine::Render
