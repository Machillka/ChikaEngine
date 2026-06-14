#pragma once

#include "ChikaEngine/ResourceHandle.hpp"
#include "ChikaEngine/base/HandleTemplate.h"
#include "ChikaEngine/base/SlotMap.h"
#include "ChikaEngine/math/Bounds.hpp"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace ChikaEngine::Render
{
    struct RenderObjectTag
    {
    };
    struct RenderLightTag
    {
    };
    struct RenderViewTag
    {
    };

    using RenderObjectHandle = Core::THandle<RenderObjectTag>;
    using RenderLightHandle = Core::THandle<RenderLightTag>;
    using RenderViewHandle = Core::THandle<RenderViewTag>;

    /**
     * @brief 描述 Render Proxy 参与哪些渲染路径。
     *
     * Flags 属于 RenderWorld，不暴露 Gameplay Component 类型，为后续 Pass 分类提供稳定输入。
     */
    enum class RenderObjectFlags : uint32_t
    {
        None = 0,
        Visible = 1u << 0,
        CastShadow = 1u << 1,
        Skinned = 1u << 2,
        Transparent = 1u << 3,
        Masked = 1u << 4,
    };

    RenderObjectFlags operator|(RenderObjectFlags lhs, RenderObjectFlags rhs);
    bool HasFlag(RenderObjectFlags flags, RenderObjectFlags flag);

    /**
     * @brief 保存 Render Proxy 的空间范围占位。
     *
     * Phase 2 建立数据边界；Phase 3 将由 Mesh Importer 计算并更新有效 Bounds。
     */
    using RenderBounds = Math::Bounds;

    /**
     * @brief Renderer 消费的稳定对象代理，等价于简化的 Unreal Primitive Scene Proxy。
     *
     * Proxy 只保存渲染数据与 Resource Handle，不依赖 Scene、GameObject 或 Component。
     */
    struct RenderObjectProxy
    {
        Math::Mat4 transform = Math::Mat4::Identity();
        Resource::MeshHandle mesh;
        Resource::MaterialHandle material;
        RenderBounds bounds;
        RenderObjectFlags flags = RenderObjectFlags::Visible | RenderObjectFlags::CastShadow;
        uint32_t layerMask = 0xFFFFFFFFu;
        std::vector<Math::Mat4> boneMatrices;
    };

    enum class RenderLightType : uint32_t
    {
        Directional,
        Point,
        Spot,
    };

    /**
     * @brief 保存渲染光源数据，避免 Renderer 使用特殊的全局光状态。
     */
    struct RenderLightProxy
    {
        RenderLightType type = RenderLightType::Directional;
        Math::Vector3 position{};
        Math::Vector3 direction{ 0.5f, -1.0f, 0.3f };
        Math::Vector3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float range = 0.0f;
        float innerConeCos = 0.9f;
        float outerConeCos = 0.8f;
        Math::Mat4 viewProjection = Math::Mat4::Identity();
        uint32_t layerMask = 0xFFFFFFFFu;
        bool castsShadow = true;
    };

    /**
     * @brief 保存单个 Render View 的只读帧输入。
     */
    struct RenderView
    {
        Math::Mat4 view = Math::Mat4::Identity();
        Math::Mat4 projection = Math::Mat4::Identity();
        Math::Mat4 viewProjection = Math::Mat4::Identity();
        Math::Vector3 position{};
        uint32_t layerMask = 0xFFFFFFFFu;
        bool primary = false;
    };

    struct RenderObjectSnapshot
    {
        RenderObjectHandle handle;
        RenderObjectProxy proxy;
    };

    struct RenderLightSnapshot
    {
        RenderLightHandle handle;
        RenderLightProxy proxy;
    };

    struct RenderViewSnapshot
    {
        RenderViewHandle handle;
        RenderView view;
    };

    /**
     * @brief 保存一组同帧 View，为主视口、编辑器视口和 Shadow View 提供统一入口。
     */
    struct ViewFamily
    {
        std::vector<RenderViewSnapshot> views;

        const RenderView* GetPrimaryView() const;
    };

    /**
     * @brief RenderWorld 在帧边界生成的不可变消费数据。
     *
     * Snapshot 拥有数据副本。创建完成后 Renderer 只持有 const shared_ptr，不读取可变 RenderWorld。
     */
    struct RenderWorldSnapshot
    {
        uint64_t worldId = 0;
        uint64_t revision = 0;
        std::vector<RenderObjectSnapshot> objects;
        std::vector<RenderLightSnapshot> lights;
        ViewFamily viewFamily;
    };

    /**
     * @brief 保存与 Gameplay 解耦的可变渲染世界。
     *
     * 稳定 SlotMap Handle 对应 Unreal FScene 中 Scene Proxy 的身份；Bridge 负责增量更新，
     * Renderer 仅消费 CreateSnapshot() 生成的不可变帧数据。
     */
    class RenderWorld
    {
      public:
        RenderWorld();
        RenderWorld(const RenderWorld&) = delete;
        RenderWorld& operator=(const RenderWorld&) = delete;
        RenderWorld(RenderWorld&&) = delete;
        RenderWorld& operator=(RenderWorld&&) = delete;

        /** @brief 创建具有稳定 generation Handle 的渲染对象代理。 */
        RenderObjectHandle CreateObject(const RenderObjectProxy& proxy);
        /** @brief 仅在代理值变化时更新对象并推进 World Revision。 */
        bool UpdateObject(RenderObjectHandle handle, const RenderObjectProxy& proxy);
        /** @brief 删除对象并使旧 generation Handle 失效。 */
        bool DestroyObject(RenderObjectHandle handle);
        const RenderObjectProxy* GetObject(RenderObjectHandle handle) const;

        RenderLightHandle CreateLight(const RenderLightProxy& proxy);
        bool UpdateLight(RenderLightHandle handle, const RenderLightProxy& proxy);
        bool DestroyLight(RenderLightHandle handle);

        RenderViewHandle CreateView(const RenderView& view);
        bool UpdateView(RenderViewHandle handle, const RenderView& view);
        bool DestroyView(RenderViewHandle handle);

        /** @brief 在帧边界复制只读输入，使 Renderer 不访问可变 RenderWorld。 */
        std::shared_ptr<const RenderWorldSnapshot> CreateSnapshot() const;
        /** @brief 清空全部代理并推进 Revision，用于 Scene 生命周期结束。 */
        void Clear();

        uint64_t GetRevision() const
        {
            return m_revision;
        }
        uint32_t GetObjectCount() const
        {
            return m_objects.Size();
        }

      private:
        void MarkChanged();

        Core::SlotMap<RenderObjectHandle, RenderObjectProxy> m_objects;
        Core::SlotMap<RenderLightHandle, RenderLightProxy> m_lights;
        Core::SlotMap<RenderViewHandle, RenderView> m_views;
        uint64_t m_worldId = 0;
        uint64_t m_revision = 0;
    };
} // namespace ChikaEngine::Render
