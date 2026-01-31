#pragma once
#include "component/Transform.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace ChikaEngine::Framework
{
    using GameObjectID = std::uint32_t;
    class GameObject
    {
      public:
        GameObject();
        Transform* transform;
        template <typename T, typename... Args> T* AddComponent(Args&&... args);
        template <typename T> T* GetComponent();
        const std::vector<std::unique_ptr<Component>>& GetAllComponents() const;

        // TODO: 添加生命周期函数
      private:
        std::vector<std::unique_ptr<Component>> _components;
    };
} // namespace ChikaEngine::Framework