#include "ChikaEngine/RenderWorld.hpp"

#include <algorithm>
#include <atomic>

namespace ChikaEngine::Render
{
    namespace
    {
        /**
         * @brief 比较矩阵值，避免 Render Bridge 对未变化 Transform 产生无效更新。
         */
        bool Equal(const Math::Mat4& lhs, const Math::Mat4& rhs)
        {
            return lhs.m == rhs.m;
        }

        bool Equal(const Math::Vector3& lhs, const Math::Vector3& rhs)
        {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        }

        bool Equal(const RenderBounds& lhs, const RenderBounds& rhs)
        {
            return Equal(lhs.center, rhs.center) && Equal(lhs.extents, rhs.extents) && lhs.sphereRadius == rhs.sphereRadius && lhs.valid == rhs.valid;
        }

        bool Equal(const std::vector<Math::Mat4>& lhs, const std::vector<Math::Mat4>& rhs)
        {
            return lhs.size() == rhs.size() && std::ranges::equal(lhs, rhs, [](const Math::Mat4& a, const Math::Mat4& b) { return Equal(a, b); });
        }

        bool Equal(const RenderObjectProxy& lhs, const RenderObjectProxy& rhs)
        {
            return Equal(lhs.transform, rhs.transform) && lhs.mesh == rhs.mesh && lhs.material == rhs.material && Equal(lhs.bounds, rhs.bounds) && lhs.flags == rhs.flags && lhs.layerMask == rhs.layerMask && Equal(lhs.boneMatrices, rhs.boneMatrices);
        }

        bool Equal(const RenderLightProxy& lhs, const RenderLightProxy& rhs)
        {
            return lhs.type == rhs.type && Equal(lhs.position, rhs.position) && Equal(lhs.direction, rhs.direction) && Equal(lhs.color, rhs.color) && lhs.intensity == rhs.intensity && lhs.range == rhs.range && Equal(lhs.viewProjection, rhs.viewProjection) && lhs.layerMask == rhs.layerMask && lhs.castsShadow == rhs.castsShadow;
        }

        bool Equal(const RenderView& lhs, const RenderView& rhs)
        {
            return Equal(lhs.view, rhs.view) && Equal(lhs.projection, rhs.projection) && Equal(lhs.viewProjection, rhs.viewProjection) && Equal(lhs.position, rhs.position) && lhs.layerMask == rhs.layerMask && lhs.primary == rhs.primary;
        }
    } // namespace

    RenderWorld::RenderWorld()
    {
        static std::atomic<uint64_t> nextWorldId = 0;
        m_worldId = ++nextWorldId;
    }

    RenderObjectFlags operator|(RenderObjectFlags lhs, RenderObjectFlags rhs)
    {
        return static_cast<RenderObjectFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    bool HasFlag(RenderObjectFlags flags, RenderObjectFlags flag)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
    }

    const RenderView* ViewFamily::GetPrimaryView() const
    {
        const auto primary = std::ranges::find(views, true, [](const RenderViewSnapshot& snapshot) { return snapshot.view.primary; });
        if (primary != views.end())
            return &primary->view;
        return views.empty() ? nullptr : &views.front().view;
    }

    RenderObjectHandle RenderWorld::CreateObject(const RenderObjectProxy& proxy)
    {
        MarkChanged();
        return m_objects.Create(proxy);
    }

    bool RenderWorld::UpdateObject(RenderObjectHandle handle, const RenderObjectProxy& proxy)
    {
        RenderObjectProxy* current = m_objects.Get(handle);
        if (!current || Equal(*current, proxy))
            return false;
        *current = proxy;
        MarkChanged();
        return true;
    }

    bool RenderWorld::DestroyObject(RenderObjectHandle handle)
    {
        if (!m_objects.Get(handle))
            return false;
        m_objects.Destroy(handle);
        MarkChanged();
        return true;
    }

    const RenderObjectProxy* RenderWorld::GetObject(RenderObjectHandle handle) const
    {
        return m_objects.Get(handle);
    }

    RenderLightHandle RenderWorld::CreateLight(const RenderLightProxy& proxy)
    {
        MarkChanged();
        return m_lights.Create(proxy);
    }

    bool RenderWorld::UpdateLight(RenderLightHandle handle, const RenderLightProxy& proxy)
    {
        RenderLightProxy* current = m_lights.Get(handle);
        if (!current || Equal(*current, proxy))
            return false;
        *current = proxy;
        MarkChanged();
        return true;
    }

    bool RenderWorld::DestroyLight(RenderLightHandle handle)
    {
        if (!m_lights.Get(handle))
            return false;
        m_lights.Destroy(handle);
        MarkChanged();
        return true;
    }

    RenderViewHandle RenderWorld::CreateView(const RenderView& view)
    {
        MarkChanged();
        return m_views.Create(view);
    }

    bool RenderWorld::UpdateView(RenderViewHandle handle, const RenderView& view)
    {
        RenderView* current = m_views.Get(handle);
        if (!current || Equal(*current, view))
            return false;
        *current = view;
        MarkChanged();
        return true;
    }

    bool RenderWorld::DestroyView(RenderViewHandle handle)
    {
        if (!m_views.Get(handle))
            return false;
        m_views.Destroy(handle);
        MarkChanged();
        return true;
    }

    std::shared_ptr<const RenderWorldSnapshot> RenderWorld::CreateSnapshot() const
    {
        auto snapshot = std::make_shared<RenderWorldSnapshot>();
        snapshot->worldId = m_worldId;
        snapshot->revision = m_revision;
        m_objects.ForEach([&](RenderObjectHandle handle, const RenderObjectProxy& proxy) { snapshot->objects.push_back({ handle, proxy }); });
        m_lights.ForEach([&](RenderLightHandle handle, const RenderLightProxy& proxy) { snapshot->lights.push_back({ handle, proxy }); });
        m_views.ForEach([&](RenderViewHandle handle, const RenderView& view) { snapshot->viewFamily.views.push_back({ handle, view }); });
        return snapshot;
    }

    void RenderWorld::Clear()
    {
        if (m_objects.Size() == 0 && m_lights.Size() == 0 && m_views.Size() == 0)
            return;
        m_objects.Clear();
        m_lights.Clear();
        m_views.Clear();
        MarkChanged();
    }

    void RenderWorld::MarkChanged()
    {
        ++m_revision;
    }
} // namespace ChikaEngine::Render
