/*!
 * @file rendersubsystem.h
 * @author Machillka (machillka2007@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-02-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/RenderWorld.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/event/EventBus.hpp"
#include "ChikaEngine/subsystem/ISubsystem.h"
#include <unordered_map>
namespace ChikaEngine::Framework
{
    class Scene;
    class MeshRenderer;

    /**
     * @brief 将 Gameplay 生命周期增量同步为 RenderWorld Scene Proxy。
     *
     * 该类对应简化的 Unreal Scene Bridge：它可以读取 Gameplay，但 RenderWorld 和 Renderer
     * 不反向依赖 Scene、GameObject 或 Component。
     */
    class RenderSubsystem : public ISubsystem
    {
      public:
        RenderSubsystem(Scene* ownerScene, Render::Renderer* renderInstance);
        ~RenderSubsystem() override;
        /** @brief 同步已注册组件的变化，并在帧边界提交不可变 Snapshot。 */
        void Tick(float deltaTime) override;
        /** @brief 解除事件订阅并向 Renderer 提交空世界，避免悬空代理。 */
        void Cleanup() override;

        const Render::RenderWorld& GetRenderWorld() const
        {
            return _renderWorld;
        }
        uint32_t GetLastProxyUpdateCount() const
        {
            return _lastProxyUpdateCount;
        }

      private:
        struct ProxyEntry
        {
            MeshRenderer* component = nullptr;
            Render::RenderObjectHandle renderObject;
            Asset::MeshHandle meshAsset;
            Asset::MaterialHandle materialAsset;
            Resource::MeshHandle meshResource;
            Resource::MaterialHandle materialResource;
            bool resourcesDirty = true;
        };

        void SubscribeSceneEvents();
        /** @brief 仅在 Bridge 创建时扫描一次现有组件，后续依赖生命周期事件。 */
        void RegisterExistingComponents();
        void RegisterMeshRenderer(Core::GameObjectID gameObjectId, MeshRenderer* component);
        void RemoveEntry(Core::GameObjectID gameObjectId);
        void DeactivateEntry(ProxyEntry& entry);
        /** @brief 只 Resolve Dirty 资产，并把对象值变化增量写入 RenderWorld。 */
        void SyncEntry(Core::GameObjectID gameObjectId, ProxyEntry& entry);
        void SyncViewAndLight();

        Scene* _ownerScene = nullptr;
        Render::Renderer* _renderer;
        Asset::AssetManager* _assetMgr = nullptr;
        Resource::ResourceManager* _resourceMgr = nullptr;
        Render::RenderWorld _renderWorld;
        Render::RenderViewHandle _primaryView;
        Render::RenderLightHandle _defaultLight;
        std::unordered_map<Core::GameObjectID, ProxyEntry> _entries;
        std::vector<EventBus::SubscriptionId> _eventSubscriptions;
        size_t _assetReloadSubscription = 0;
        uint32_t _lastProxyUpdateCount = 0;
        bool _cleanedUp = false;
    };
} // namespace ChikaEngine::Framework
