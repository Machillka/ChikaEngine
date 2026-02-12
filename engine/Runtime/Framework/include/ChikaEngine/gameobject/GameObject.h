#pragma once

#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/debug/log_macros.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ChikaEngine::Framework
{
    using GameObjectID = std::uint64_t; // 使用 64bits 符合雪花算法

    // TODO[x]: 完善这个简陋的自增 ID
    // FIXME: 自增逻辑移动到 scene manager 中

    // inline static GameObjectID idCount = 0;
    class GameObject
    {
      public:
        GameObject(std::string name, GameObjectID id);
        ~GameObject();
        // 基本信息 —— ID(全局唯一) 和 名字 (随意)
        GameObjectID GetID() const
        {
            return _id;
        }
        void SetName(const std::string& name)
        {
            _name = name;
        }
        const std::string& GetName()
        {
            return _name;
        }

        // 必须含有 transform
        Transform* transform = nullptr;

        // 提供组件相关方法
        template <typename T, typename... Args> T* AddComponent(Args&&... args)
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
            {
                ptr->OnEnable();
                LOG_INFO("GameObject", "OnEnable Function Called");
            }

            return ptr;
        }

        template <typename T> T* GetComponent()
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

        const std::vector<std::unique_ptr<Component>>& GetAllComponents() const;

        void SetActive(bool active);
        bool IsActive() const noexcept
        {
            return _active;
        }

        // TODO[x]: 添加生命周期函数
        void Start();
        void Update(float deltaTime);
        void FixedUpdate(float fixedDeltaTime);
        // TODO: 加入 lateupdate

      private:
        GameObjectID _id;
        std::string _name;

        std::vector<std::unique_ptr<Component>> _components;
        // 加入对于组件的锁
        mutable std::mutex _compMutex;

        bool _active = true;
        // 是否执行过 start 方法
        bool _started = false;
    };
} // namespace ChikaEngine::Framework