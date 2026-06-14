#include "ChikaEngine/subsystem/RenderSubsystem.h"

#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/scene/SceneEvents.hpp"
#include "ChikaEngine/scene/scene.hpp"

#include <string_view>

namespace ChikaEngine::Framework
{
    RenderSubsystem::RenderSubsystem(Scene* ownerScene, Render::Renderer* renderInstance) : _ownerScene(ownerScene), _renderer(renderInstance)
    {
        _assetMgr = _renderer->GetAssetManager();
        _resourceMgr = _renderer->GetResourceManager();
        SubscribeSceneEvents();
        RegisterExistingComponents();

        using namespace Math;
        const Vector3 lightPosition(5.0f, 8.0f, 5.0f);
        Mat4 lightProjection = Mat4::Perspective(3.1415926f / 3.0f, 1.0f, 0.1f, 100.0f);
        lightProjection(1, 1) *= -1.0f;
        lightProjection(2, 2) = -100.0f / (100.0f - 0.1f);
        lightProjection(2, 3) = -(100.0f * 0.1f) / (100.0f - 0.1f);
        _defaultLight = _renderWorld.CreateLight({
            .type = Render::RenderLightType::Directional,
            .position = lightPosition,
            .direction = lightPosition,
            .viewProjection = lightProjection * Mat4::LookAt(lightPosition, Vector3(0.0f, 0.0f, 0.0f), Vector3::up),
        });
        _primaryView = _renderWorld.CreateView(_renderer->CreateEditorView());

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
        _lastProxyUpdateCount = 0;
        for (auto& [gameObjectId, entry] : _entries)
            SyncEntry(gameObjectId, entry);
        SyncViewAndLight();
        _renderer->SubmitRenderWorldSnapshot(_renderWorld.CreateSnapshot());
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
            }));
        _eventSubscriptions.push_back(events.Subscribe<ComponentRemovedEvent>(
            [this](const ComponentRemovedEvent& event)
            {
                if (event.componentType && std::string_view(event.componentType) == "MeshRenderer")
                    RemoveEntry(event.gameObjectId);
            }));
        _eventSubscriptions.push_back(events.Subscribe<GameObjectDestroyedEvent>([this](const GameObjectDestroyedEvent& event) { RemoveEntry(event.gameObjectId); }));
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
            }));
    }

    void RenderSubsystem::RegisterExistingComponents()
    {
        for (const auto& gameObject : _ownerScene->GetAllGameobjects())
        {
            if (auto* meshRenderer = gameObject->GetComponent<MeshRenderer>())
                RegisterMeshRenderer(gameObject->GetID(), meshRenderer);
        }
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

        Render::RenderObjectProxy proxy{
            .transform = owner->transform ? owner->transform->GetWorldMat() : Math::Mat4::Identity(),
            .mesh = entry.meshResource,
            .material = entry.materialResource,
        };
        if (const auto* animator = owner->GetComponent<Animator>(); animator && !animator->finalMatrices.empty())
        {
            proxy.flags = proxy.flags | Render::RenderObjectFlags::Skinned;
            proxy.boneMatrices = animator->finalMatrices;
        }

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

    void RenderSubsystem::SyncViewAndLight()
    {
        if (_primaryView.IsValid() && _renderWorld.UpdateView(_primaryView, _renderer->CreateEditorView()))
            ++_lastProxyUpdateCount;
    }
} // namespace ChikaEngine::Framework
