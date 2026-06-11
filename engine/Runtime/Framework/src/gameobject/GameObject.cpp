#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/scene/SceneEvents.hpp"
#include "ChikaEngine/scene/scene.hpp"
#include <algorithm>
#include <memory>
#include <string>

namespace ChikaEngine::Framework
{
    GameObject::GameObject(Core::GameObjectID id, std::string name, Scene* ownerScene) : _id(id), _name(name), _scene(ownerScene)
    {
        transform = this->AddComponent<Transform>();
    }

    GameObject::~GameObject()
    {
        PrepareDestroy();
    }

    void GameObject::InitializeComponent(Component& component)
    {
        component.OnValidate();
        if (!component._isAwake)
        {
            component._isAwake = true;
            component.Awake();
        }

        RefreshComponentActivation(component);
        if (_isPlaying && component._isActiveAndEnabled && !component._isStarted)
        {
            component._isStarted = true;
            component.Start();
        }

        if (_scene)
            _scene->GetEventBus().Publish(ComponentAddedEvent{ .scene = _scene, .gameObjectId = _id, .component = &component });
    }

    bool GameObject::RemoveComponent(Component* component)
    {
        if (!component || component == transform)
            return false;

        const auto it = std::find_if(_components.begin(), _components.end(), [component](const auto& owned) { return owned.get() == component; });
        if (it == _components.end())
            return false;

        if (_isIteratingComponents)
        {
            if (std::find(_pendingComponentRemovals.begin(), _pendingComponentRemovals.end(), component) == _pendingComponentRemovals.end())
                _pendingComponentRemovals.push_back(component);
            return true;
        }

        DestroyComponent(*component);
        std::erase_if(_components, [component](const auto& owned) { return owned.get() == component; });
        return true;
    }

    void GameObject::DestroyComponent(Component& component)
    {
        if (component._isDestroyed)
            return;

        if (component._isActiveAndEnabled)
        {
            component._isActiveAndEnabled = false;
            component.OnDisable();
        }

        component._isDestroyed = true;
        component.OnDestroy();

        if (_scene)
        {
            _scene->GetEventBus().Publish(ComponentRemovedEvent{
                .scene = _scene,
                .gameObjectId = _id,
                .componentType = component.GetReflectedClassName().c_str(),
            });
        }
    }

    void GameObject::FlushPendingComponentRemovals()
    {
        if (_isIteratingComponents || _pendingComponentRemovals.empty())
            return;

        const auto pending = std::move(_pendingComponentRemovals);
        _pendingComponentRemovals.clear();
        for (auto* component : pending)
            RemoveComponent(component);
    }

    const std::vector<std::unique_ptr<Component>>& GameObject::GetAllComponents() const
    {
        return _components;
    }

    void GameObject::BeginPlay()
    {
        if (_isPlaying || _pendingDestroy)
            return;

        _isPlaying = true;
        _isIteratingComponents = true;
        for (auto& component : _components)
        {
            if (_pendingDestroy)
                break;
            if (component->_isActiveAndEnabled && !component->_isStarted)
            {
                component->_isStarted = true;
                component->Start();
            }
        }
        _isIteratingComponents = false;
        FlushPendingComponentRemovals();
    }

    void GameObject::EndPlay()
    {
        if (!_isPlaying)
            return;

        _isPlaying = false;
        for (auto& component : _components)
            component->_isStarted = false;
    }

    void GameObject::FixedTick(float fixedDeltaTime)
    {
        if (!_isPlaying || _pendingDestroy || !IsActiveInHierarchy())
            return;

        _isIteratingComponents = true;
        for (const auto& component : _components)
        {
            if (_pendingDestroy)
                break;
            if (component->_isActiveAndEnabled && component->_isStarted)
                component->FixedTick(fixedDeltaTime);
        }
        _isIteratingComponents = false;
        FlushPendingComponentRemovals();
    }

    void GameObject::Tick(float deltaTime)
    {
        if (!_isPlaying || _pendingDestroy || !IsActiveInHierarchy())
            return;

        _isIteratingComponents = true;
        for (const auto& component : _components)
        {
            if (_pendingDestroy)
                break;
            if (component->_isActiveAndEnabled && component->_isStarted)
                component->Tick(deltaTime);
        }
        _isIteratingComponents = false;
        FlushPendingComponentRemovals();
    }

    void GameObject::LateTick(float deltaTime)
    {
        if (!_isPlaying || _pendingDestroy || !IsActiveInHierarchy())
            return;

        _isIteratingComponents = true;
        for (const auto& component : _components)
        {
            if (_pendingDestroy)
                break;
            if (component->_isActiveAndEnabled && component->_isStarted)
                component->LateTick(deltaTime);
        }
        _isIteratingComponents = false;
        FlushPendingComponentRemovals();
    }

    void GameObject::SetActive(bool active)
    {
        if (_active == active)
            return;
        _active = active;

        RefreshActiveInHierarchy();
    }

    bool GameObject::IsActiveInHierarchy() const
    {
        if (!_active)
            return false;

        if (!transform || !transform->GetParent())
            return true;

        const auto* parentOwner = transform->GetParent()->GetOwner();
        return parentOwner && parentOwner->IsActiveInHierarchy();
    }

    void GameObject::RefreshComponentActivation(Component& component)
    {
        const bool shouldBeActive = !_pendingDestroy && component._isEnabled && IsActiveInHierarchy();
        if (component._isActiveAndEnabled == shouldBeActive)
            return;

        component._isActiveAndEnabled = shouldBeActive;
        if (shouldBeActive)
        {
            component.OnEnable();
            if (_isPlaying && component._isActiveAndEnabled && !component._isStarted)
            {
                component._isStarted = true;
                component.Start();
            }
        }
        else
            component.OnDisable();
    }

    void GameObject::RefreshActiveInHierarchy()
    {
        for (auto& component : _components)
            RefreshComponentActivation(*component);

        if (!transform)
            return;
        for (auto* child : transform->GetChildren())
        {
            if (child && child->GetOwner())
                child->GetOwner()->RefreshActiveInHierarchy();
        }
    }

    void GameObject::OnDeserialized()
    {
        transform = this->GetComponent<Transform>();

        for (auto& component : _components)
            InitializeComponent(*component);
    }

    void GameObject::PrepareDestroy()
    {
        if (_pendingDestroy && _components.empty())
            return;

        _pendingDestroy = true;
        EndPlay();

        for (auto& component : _components)
            DestroyComponent(*component);

        _pendingComponentRemovals.clear();
        _components.clear();
        transform = nullptr;
    }
} // namespace ChikaEngine::Framework
