#include "GameObject.h"

#include "debug/log_macros.h"
#include "framework/component/Component.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <utility>

namespace ChikaEngine::Framework
{
    GameObject::GameObject()
    {
        _id = idCount++;
        transform = this->AddComponent<Transform>();
        LOG_INFO("GameObject", "Create GO with ID", _id);
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

    template <typename T, typename... Args> T* GameObject::AddComponent(Args&&... args)
    {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->SetOwner(this);
        component->Awake();
        T* ptr = component.get();
        // 移交所有权
        {
            std::lock_guard lock(_compMutex);
            _components.emplace_back(std::move(component));
        }

        if (_active && ptr->IsEnabled())
            ptr->OnEnable();

        return ptr;
    }

    template <typename T> T* GameObject::GetComponent()
    {
        std::lock_guard lock(_compMutex);
        for (auto& comp : _components)
        {
            if (auto p = dynamic_cast<T*>(comp.get()))
            {
                return p;
            }
        }

        return nullptr;
    }

    const std::vector<std::unique_ptr<Component>>& GameObject::GetAllComponents() const
    {
        std::lock_guard lock(_compMutex);
        return _components;
    }

    template <typename T> bool GameObject::RemoveComponent()
    {
        std::lock_guard lock(_compMutex);
        auto it = std::find_if(_components.begin(),
                               _components.end(),
                               [](const std::unique_ptr<Component>& c)
                               {
                                   // 返回可以动态转化成目标类型的组件(说明匹配到对应类型了)
                                   return dynamic_cast<T*>(c);
                               });
        if (it != _components.end())
        {
            (*it)->OnDestroy();
            _components.erase(it);
            return true;
        }
        return false;
    }

    void GameObject::SetActive(bool active)
    {
        std::lock_guard lock(_compMutex);
        // 不改变状态
        if (active == _active)
            return;

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