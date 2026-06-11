#pragma once

#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace ChikaEngine::Framework
{
    // TODO[x]: 完善这个简陋的自增 ID

    class Scene; // 前向声明
    MCLASS(GameObject)
    {
        REFLECTION_BODY(GameObject);

      public:
        // 创建空的占位物体
        GameObject() = default;
        GameObject(Core::GameObjectID id, std::string name = "GameObject", Scene* ownerScene = nullptr);
        ~GameObject();

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;
        GameObject(GameObject&&) = delete;
        GameObject& operator=(GameObject&&) = delete;

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
        const std::string& GetName() const
        {
            return _name;
        }

        // 必须含有 transform
        MFIELD()
        Framework::Transform* transform = nullptr;

        // 提供组件相关方法
        template <typename T, typename... Args> T* AddComponent(Args && ... args)
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
            auto component = std::make_unique<T>(std::forward<Args>(args)...);
            component->SetOwner(this);
            component->SetReflectedClassName(T::GetClassName());
            T* ptr = component.get();
            _components.emplace_back(std::move(component));
            InitializeComponent(*ptr);
            return ptr;
        }

        bool RemoveComponent(Component * component);

        template <typename T> bool RemoveComponent()
        {
            return RemoveComponent(GetComponent<T>());
        }

        template <typename T> T* GetComponent() const
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

        const std::vector<std::unique_ptr<Component>>& GetAllComponents() const;

        // 序列化相关方法
        template <typename Archive> void serialize(Archive & ar)
        {
            ar("ID", _id);
            ar("Name", _name);
            ar("Active", _active);
            Core::GameObjectID parentId = transform ? transform->GetParentId() : Core::InvalidGameObjectID;
            ar("Parent", parentId);

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

            if constexpr (Archive::IsLoading)
            {
                transform = GetComponent<Transform>();
                if (transform)
                    transform->SetSerializedParentId(parentId);
            }
        }

        // 用于反序列化后恢复内部指针状态
        void OnDeserialized();

        void SetActive(bool active);
        bool IsActive() const noexcept
        {
            return _active;
        }

        bool IsActiveInHierarchy() const;
        bool IsPlaying() const
        {
            return _isPlaying;
        }
        bool IsPendingDestroy() const
        {
            return _pendingDestroy;
        }

        void BeginPlay();
        void EndPlay();
        void FixedTick(float fixedDeltaTime);
        void Tick(float deltaTime);
        void LateTick(float deltaTime);
        void FlushPendingComponentRemovals();
        void PrepareDestroy();
        void RefreshComponentActivation(Component & component);
        void RefreshActiveInHierarchy();

        void SetScene(Scene * scene)
        {
            _scene = scene;
        }

        Scene* GetScene() const
        {
            return _scene;
        }

      protected:
        MFIELD()
        Core::GameObjectID _id = Core::InvalidGameObjectID;

        MFIELD()
        std::string _name;

        std::vector<std::unique_ptr<Component>> _components;
        Scene* _scene = nullptr;
        bool _active = true;
        bool _isPlaying = false;
        bool _pendingDestroy = false;

      private:
        friend class Scene;
        friend class Prefab;

        void InitializeComponent(Component & component);
        void DestroyComponent(Component & component);
        void MarkPendingDestroy()
        {
            _pendingDestroy = true;
        }
        void SetID(Core::GameObjectID id)
        {
            _id = id;
        }

        std::vector<Component*> _pendingComponentRemovals;
        bool _isIteratingComponents = false;
    };
} // namespace ChikaEngine::Framework
