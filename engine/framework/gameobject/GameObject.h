#pragma once
#include "framework/component/Transform.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ChikaEngine::Framework
{
    using GameObjectID = std::uint32_t;

    // TODO: 完善这个简陋的自增 ID
    // FIXME: 自增逻辑移动到 scene manager 中

    static GameObjectID idCount = 0;
    class GameObject
    {
      public:
        GameObject();
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
        template <typename T, typename... Args> T* AddComponent(Args&&... args);
        template <typename T> T* GetComponent();
        const std::vector<std::unique_ptr<Component>>& GetAllComponents() const;
        template <typename T> bool RemoveComponent();

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
        std::mutex _compMutex;

        bool _active = true;
        // 是否执行过 start 方法
        bool _started = false;
    };
} // namespace ChikaEngine::Framework