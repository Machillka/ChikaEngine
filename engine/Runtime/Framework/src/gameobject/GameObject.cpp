#include "ChikaEngine/gameobject/GameObject.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace ChikaEngine::Framework
{
    GameObject::GameObject(std::string name, GameObjectID id) : _name(name), _id(id)
    {
        transform = this->AddComponent<Transform>();
    }

    GameObject::~GameObject()
    {
        for (auto& comp : _components)
        {
            comp->OnDestroy();
        }
        _components.clear();
        LOG_INFO("GameObject", "Destroy GO, ID={}", _id);
    }

    const std::vector<std::unique_ptr<Component>>& GameObject::GetAllComponents() const
    {
        std::lock_guard lock(_compMutex);
        return _components;
    }

    void GameObject::SetActive(bool active)
    {
        std::lock_guard lock(_compMutex);
        // 不改变状态
        if (active == _active)
            return;

        _active = active;

        if (_active)
        {
            for (const auto& comp : _components)
            {
                // 不修改comp内部的状态, 仅调用启用和禁用方法
                if (comp->IsEnabled())
                    comp->OnEnable();
            }
        }
        else
        {
            for (const auto& comp : _components)
            {
                if (comp->IsEnabled())
                    comp->OnDisable();
            }
        }
    }

    void GameObject::Start()
    {
        std::lock_guard lock(_compMutex);
        if (_started)
            return;
        _started = true;
        for (const auto& comp : _components)
        {
            // Start 已经自查过是否重复执行 这里只需要组件启用即可
            if (comp->IsEnabled())
                comp->Start();
        }
    }

    void GameObject::Update(float deltaTime)
    {
        // 保证在 Update 前先 Start 过
        Start();
        std::lock_guard lock(_compMutex);

        for (const auto& comp : _components)
        {
            if (comp->IsEnabled())
                comp->Update(deltaTime);
        }
    }

    void GameObject::FixedUpdate(float fixedDeltaTime)
    {
        std::lock_guard lock(_compMutex);
        for (const auto& comp : _components)
        {
            if (comp->IsEnabled())
                comp->FixedUpdate(fixedDeltaTime);
        }
    }
} // namespace ChikaEngine::Framework