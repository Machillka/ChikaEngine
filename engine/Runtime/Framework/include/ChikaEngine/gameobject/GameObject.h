#pragma once

#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/serialization/Access.h"
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ChikaEngine::Framework
{
    // TODO[x]: 完善这个简陋的自增 ID

    class Scene; // 前向声明
    MCLASS(GameObject)
    {
        REFLECTION_BODY(GameObject);

      public:
        GameObject(std::string name = "GameObject");
        ~GameObject();
        // 基本信息 —— ID(全局唯一) 和 名字 (随意)
        MFUNCTION()
        Core::GameObjectID GetID() const
        {
            return _id;
        }
        MFUNCTION()
        void SetName(const std::string& name)
        {
            _name = name;
        }
        MFUNCTION()
        const std::string& GetName()
        {
            return _name;
        }

        // 必须含有 transform
        MFIELD()
        Framework::Transform* transform = nullptr;

        // 提供组件相关方法
        template <typename T, typename... Args> T* AddComponent(Args && ... args)
        {
            auto component = std::make_unique<T>(std::forward<Args>(args)...);
            component->SetOwner(this);
            component->SetReflectedClassName(T::GetClassName());
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

        // 序列化相关方法
        template <typename Archive> void serialize(Archive & ar)
        {
            ar("ID", _id);
            ar("Name", _name);
            ar("Active", _active);

            size_t compCount = _components.size();
            ar.EnterArray("Components", compCount);

            if constexpr (Archive::IsSaving)
            {
                for (auto& comp : _components)
                {
                    ar.EnterNode(nullptr);
                    std::string typeName = comp->GetReflectedClassName();
                    ar.ProcessValue("TypeName", typeName);
                    ar("CompData", *comp);
                    ar.LeaveNode();
                }
            }
            else if constexpr (Archive::IsLoading)
            {
                _components.clear();
                for (size_t i = 0; i < compCount; ++i)
                {
                    ar.EnterNode(nullptr);
                    std::string typeName;
                    ar.ProcessValue("TypeName", typeName);

                    auto* rawPtr = Reflection::TypeRegister::Instance().CreateInstanceByName(typeName);
                    if (!rawPtr)
                    {
                        LOG_ERROR("Serialization", "Failed to construct component: {}", typeName);
                        rawPtr = new Component();
                    }

                    std::unique_ptr<Component> comp(static_cast<Component*>(rawPtr));
                    comp->SetOwner(this);
                    comp->SetReflectedClassName(typeName.c_str());

                    // 反序列化组件内部数据
                    ar("CompData", *comp);
                    _components.push_back(std::move(comp));
                    ar.LeaveNode();
                }
            }
            ar.LeaveArray();
        }

        // 用于反序列化后恢复内部指针状态
        void OnDeserialized();

        void SetActive(bool active);
        bool IsActive() const noexcept
        {
            return _active;
        }

        // TODO[x]: 添加生命周期函数
        void Start();
        void Update(float deltaTime);
        void FixedUpdate(float fixedDeltaTime);
        // TODO[x]: 加入 lateupdate

        void SetScene(Scene * scene)
        {
            _scene = scene;
        }
        Scene* GetScene() const
        {
            return _scene;
        }

      private:
        Core::GameObjectID _id;
        MFIELD()
        std::string _name;
        std::vector<std::unique_ptr<Component>> _components;
        // 加入对于组件的锁
        mutable std::mutex _compMutex;

        Scene* _scene = nullptr;

        bool _active = true;
        // 是否执行过 start 方法
        bool _started = false;
    };
} // namespace ChikaEngine::Framework