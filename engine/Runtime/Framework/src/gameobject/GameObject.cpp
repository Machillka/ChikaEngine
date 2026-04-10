#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include <memory>
#include <mutex>
#include <string>

namespace ChikaEngine::Framework
{
    GameObject::GameObject(Core::GameObjectID id, std::string name, Scene* ownerScene) : _id(id), _name(name), _scene(ownerScene)
    {
        transform = this->AddComponent<Transform>();
    }

    GameObject::~GameObject() = default;

    void GameObject::Awake()
    {
        std::lock_guard lock(_compMutex);
        for (auto& comp : _components)
        {
            comp->Awake();
        }
    }

    const std::vector<std::unique_ptr<Component>>& GameObject::GetAllComponents() const
    {
        std::lock_guard lock(_compMutex);
        return _components;
    }

    void GameObject::Tick(float deltaTime)
    {
        std::lock_guard lock(_compMutex);

        for (const auto& comp : _components)
        {
            if (comp->IsEnabled())
                comp->Tick(deltaTime);
        }
    }

    void GameObject::SetActive(bool active)
    {
        if (_active == active)
            return;
        _active = active;

        std::lock_guard lock(_compMutex);
        for (auto& comp : _components)
        {
            if (active && comp->IsEnabled())
                comp->OnEnable();
            else
                comp->OnDisable();
        }
    }

    void GameObject::OnDeserialized()
    {
        // 恢复裸指针
        transform = this->GetComponent<Transform>();

        for (auto& comp : _components)
        {
            if (comp->IsEnabled())
            {
                comp->OnEnable();
            }
        }
    }
} // namespace ChikaEngine::Framework