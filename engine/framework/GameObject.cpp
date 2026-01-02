#include "GameObject.h"

#include "component/Transform.h"

#include <memory>
#include <utility>

namespace ChikaEngine::Framework
{
    GameObject::GameObject()
    {
        transform = this->AddComponent<Transform>();
    }

    template <typename T, typename... Args> T* GameObject::AddComponent(Args&&... args)
    {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();
        // 移交所有权
        _components.push_back(std::move(component));
        return ptr;
    }

    template <typename T> T* GameObject::GetComponent()
    {
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
        return _components;
    }
} // namespace ChikaEngine::Framework