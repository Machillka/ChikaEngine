#include "ChikaEngine/subsystem/RenderSubsystem.h"

#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/CameraComponent.hpp"
#include "ChikaEngine/component/LightComponent.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/scene/SceneEvents.hpp"
#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"

#include <string_view>

namespace ChikaEngine::Framework
{
    RenderSubsystem::RenderSubsystem(Scene* ownerScene, Render::Renderer* renderInstance, bool useEditorView) : _ownerScene(ownerScene), _renderer(renderInstance), _useEditorView(useEditorView)
    {
        _assetMgr = _renderer->GetAssetManager();
        _resourceMgr = _renderer->GetResourceManager();
        SubscribeSceneEvents();
        RegisterExistingComponents();

        if (_useEditorView)
            _editorView = _renderWorld.CreateView(_renderer->CreateEditorView());

        _assetReloadSubscription = _assetMgr->SubscribeReload(
            [this](const Asset::AssetReloadEvent&)
            {
                for (auto& [id, entry] : _entries)
                    entry.resourcesDirty = true;
            });
    }

    RenderSubsystem::~RenderSubsystem()
    {
        Cleanup();
    }

    void RenderSubsystem::Tick(float)
    {
        CHIKA_PROFILE_SCOPE("RenderWorld.BuildSnapshot");
        _lastProxyUpdateCount = 0;
        for (auto& [gameObjectId, entry] : _entries)
            SyncEntry(gameObjectId, entry);
        SyncViewsAndLights();
        _renderer->SubmitRenderWorldSnapshot(_renderWorld.CreateSnapshot());
        CHIKA_PROFILE_COUNTER("RenderWorld.ProxyUpdates", static_cast<int64_t>(_lastProxyUpdateCount));
    }

    void RenderSubsystem::Cleanup()
    {
        if (_cleanedUp)
            return;
        _cleanedUp = true;

        if (_ownerScene)
        {
            for (const auto subscription : _eventSubscriptions)
                _ownerScene->GetEventBus().Unsubscribe(subscription);
        }
        _eventSubscriptions.clear();
        if (_assetMgr && _assetReloadSubscription != 0)
            _assetMgr->UnsubscribeReload(_assetReloadSubscription);
        _assetReloadSubscription = 0;
        _entries.clear();
        _views.clear();
        _lights.clear();
        _renderWorld.Clear();
        if (_renderer)
            _renderer->SubmitRenderWorldSnapshot(_renderWorld.CreateSnapshot());
    }

    void RenderSubsystem::SubscribeSceneEvents()
    {
        EventBus& events = _ownerScene->GetEventBus();
        _eventSubscriptions.push_back(events.Subscribe<ComponentAddedEvent>(
            [this](const ComponentAddedEvent& event)
            {
                if (auto* meshRenderer = dynamic_cast<MeshRenderer*>(event.component))
                    RegisterMeshRenderer(event.gameObjectId, meshRenderer);
                else if (auto* camera = dynamic_cast<CameraComponent*>(event.component))
                    RegisterCamera(event.gameObjectId, camera);
                else if (auto* light = dynamic_cast<LightComponent*>(event.component))
                    RegisterLight(event.gameObjectId, light);
            }));
        _eventSubscriptions.push_back(events.Subscribe<ComponentRemovedEvent>(
            [this](const ComponentRemovedEvent& event)
            {
                if (event.componentType && std::string_view(event.componentType) == "MeshRenderer")
                    RemoveEntry(event.gameObjectId);
                else if (event.componentType && std::string_view(event.componentType) == "CameraComponent")
                    RemoveView(event.gameObjectId);
                else if (event.componentType && std::string_view(event.componentType) == "LightComponent")
                    RemoveLight(event.gameObjectId);
            }));
        _eventSubscriptions.push_back(events.Subscribe<GameObjectDestroyedEvent>([this](const GameObjectDestroyedEvent& event) { RemoveGameObjectEntries(event.gameObjectId); }));
        _eventSubscriptions.push_back(events.Subscribe<ComponentActivationChangedEvent>(
            [this](const ComponentActivationChangedEvent& event)
            {
                if (auto* meshRenderer = dynamic_cast<MeshRenderer*>(event.component))
                {
                    RegisterMeshRenderer(event.gameObjectId, meshRenderer);
                    auto entry = _entries.find(event.gameObjectId);
                    if (entry != _entries.end())
                        SyncEntry(event.gameObjectId, entry->second);
                }
                else if (auto* camera = dynamic_cast<CameraComponent*>(event.component))
                    RegisterCamera(event.gameObjectId, camera);
                else if (auto* light = dynamic_cast<LightComponent*>(event.component))
                    RegisterLight(event.gameObjectId, light);
            }));
    }

    void RenderSubsystem::RegisterExistingComponents()
    {
        for (const auto& gameObject : _ownerScene->GetAllGameobjects())
        {
            if (auto* meshRenderer = gameObject->GetComponent<MeshRenderer>())
                RegisterMeshRenderer(gameObject->GetID(), meshRenderer);
            if (auto* camera = gameObject->GetComponent<CameraComponent>())
                RegisterCamera(gameObject->GetID(), camera);
            if (auto* light = gameObject->GetComponent<LightComponent>())
                RegisterLight(gameObject->GetID(), light);
        }
    }

    void RenderSubsystem::RegisterCamera(Core::GameObjectID gameObjectId, CameraComponent* component)
    {
        if (component)
            _views[gameObjectId].component = component;
    }

    void RenderSubsystem::RegisterLight(Core::GameObjectID gameObjectId, LightComponent* component)
    {
        if (component)
            _lights[gameObjectId].component = component;
    }

    void RenderSubsystem::RegisterMeshRenderer(Core::GameObjectID gameObjectId, MeshRenderer* component)
    {
        if (!component)
            return;
        auto [entryIt, inserted] = _entries.try_emplace(gameObjectId);
        ProxyEntry& entry = entryIt->second;
        if (inserted || entry.component != component)
            entry.resourcesDirty = true;
        entry.component = component;
    }

    void RenderSubsystem::RemoveView(Core::GameObjectID gameObjectId)
    {
        const auto entry = _views.find(gameObjectId);
        if (entry == _views.end())
            return;
        if (entry->second.renderView.IsValid())
            _renderWorld.DestroyView(entry->second.renderView);
        _views.erase(entry);
    }

    void RenderSubsystem::RemoveLight(Core::GameObjectID gameObjectId)
    {
        const auto entry = _lights.find(gameObjectId);
        if (entry == _lights.end())
            return;
        if (entry->second.renderLight.IsValid())
            _renderWorld.DestroyLight(entry->second.renderLight);
        _lights.erase(entry);
    }

    void RenderSubsystem::RemoveGameObjectEntries(Core::GameObjectID gameObjectId)
    {
        RemoveEntry(gameObjectId);
        RemoveView(gameObjectId);
        RemoveLight(gameObjectId);
    }

    void RenderSubsystem::RemoveEntry(Core::GameObjectID gameObjectId)
    {
        const auto entry = _entries.find(gameObjectId);
        if (entry == _entries.end())
            return;
        DeactivateEntry(entry->second);
        _entries.erase(entry);
    }

    void RenderSubsystem::DeactivateEntry(ProxyEntry& entry)
    {
        if (!entry.renderObject.IsValid())
            return;
        if (_renderWorld.DestroyObject(entry.renderObject))
            ++_lastProxyUpdateCount;
        entry.renderObject = Render::RenderObjectHandle::Invalid();
    }

    void RenderSubsystem::SyncEntry(Core::GameObjectID gameObjectId, ProxyEntry& entry)
    {
        GameObject* owner = _ownerScene->GetGameObject(gameObjectId);
        if (!owner || owner->IsPendingDestroy() || !owner->IsActiveInHierarchy() || !entry.component || !entry.component->IsActiveAndEnabled())
        {
            DeactivateEntry(entry);
            return;
        }

        if (entry.component->NeedsAssetResolve())
            entry.component->ResolveAssets(*_assetMgr);

        const Asset::MeshHandle meshAsset = entry.component->GetMeshAsset();
        const Asset::MaterialHandle materialAsset = entry.component->GetMaterialAsset();
        if (!meshAsset.IsValid() || !materialAsset.IsValid())
        {
            DeactivateEntry(entry);
            return;
        }

        if (entry.resourcesDirty || entry.meshAsset != meshAsset || entry.materialAsset != materialAsset)
        {
            entry.meshAsset = meshAsset;
            entry.materialAsset = materialAsset;
            entry.meshResource = _resourceMgr->UploadMesh(meshAsset);
            entry.materialResource = _resourceMgr->UploadMaterial(materialAsset);
            entry.resourcesDirty = false;
        }
        if (!entry.meshResource.IsValid() || !entry.materialResource.IsValid())
        {
            DeactivateEntry(entry);
            return;
        }

        const Math::Mat4 worldTransform = owner->transform ? owner->transform->GetWorldMat() : Math::Mat4::Identity();
        Render::RenderObjectProxy proxy{
            .transform = worldTransform,
            .mesh = entry.meshResource,
            .material = entry.materialResource,
            .bounds = Math::TransformBounds(_resourceMgr->GetMesh(entry.meshResource).bounds, worldTransform),
        };
        if (const auto* animator = owner->GetComponent<Animator>(); animator && !animator->finalMatrices.empty())
        {
            proxy.flags = proxy.flags | Render::RenderObjectFlags::Skinned;
            proxy.boneMatrices = animator->finalMatrices;
        }
        const Resource::MaterialGPU& material = _resourceMgr->GetMaterial(entry.materialResource);
        if (material.transparent)
            proxy.flags = proxy.flags | Render::RenderObjectFlags::Transparent;
        if (material.masked)
            proxy.flags = proxy.flags | Render::RenderObjectFlags::Masked;

        if (!entry.renderObject.IsValid())
        {
            entry.renderObject = _renderWorld.CreateObject(proxy);
            ++_lastProxyUpdateCount;
        }
        else if (_renderWorld.UpdateObject(entry.renderObject, proxy))
        {
            ++_lastProxyUpdateCount;
        }
    }

    void RenderSubsystem::SyncViewsAndLights()
    {
        if (_editorView.IsValid() && _renderWorld.UpdateView(_editorView, _renderer->CreateEditorView()))
            ++_lastProxyUpdateCount;

        const float aspectRatio = _renderer->GetViewportAspectRatio();
        for (auto& [gameObjectId, entry] : _views)
        {
            GameObject* owner = _ownerScene->GetGameObject(gameObjectId);
            const bool active = !_useEditorView && owner && owner->IsActiveInHierarchy() && !owner->IsPendingDestroy() && entry.component && entry.component->IsActiveAndEnabled();
            if (!active)
            {
                if (entry.renderView.IsValid() && _renderWorld.DestroyView(entry.renderView))
                    ++_lastProxyUpdateCount;
                entry.renderView = Render::RenderViewHandle::Invalid();
                continue;
            }

            const Render::RenderView view = entry.component->BuildRenderView(aspectRatio);
            if (!entry.renderView.IsValid())
            {
                entry.renderView = _renderWorld.CreateView(view);
                ++_lastProxyUpdateCount;
            }
            else if (_renderWorld.UpdateView(entry.renderView, view))
                ++_lastProxyUpdateCount;
        }

        for (auto& [gameObjectId, entry] : _lights)
        {
            GameObject* owner = _ownerScene->GetGameObject(gameObjectId);
            const bool active = owner && owner->IsActiveInHierarchy() && !owner->IsPendingDestroy() && entry.component && entry.component->IsActiveAndEnabled();
            if (!active)
            {
                if (entry.renderLight.IsValid() && _renderWorld.DestroyLight(entry.renderLight))
                    ++_lastProxyUpdateCount;
                entry.renderLight = Render::RenderLightHandle::Invalid();
                continue;
            }

            const Render::RenderLightProxy light = entry.component->BuildRenderLightProxy();
            if (!entry.renderLight.IsValid())
            {
                entry.renderLight = _renderWorld.CreateLight(light);
                ++_lastProxyUpdateCount;
            }
            else if (_renderWorld.UpdateLight(entry.renderLight, light))
                ++_lastProxyUpdateCount;
        }
    }
} // namespace ChikaEngine::Framework
