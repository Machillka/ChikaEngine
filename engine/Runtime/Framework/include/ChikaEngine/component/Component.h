#pragma once
#include "ChikaEngine/reflection/ReflectionMacros.h"
#include <string>

namespace ChikaEngine::Framework
{
    class GameObject;
    class Scene;
    MCLASS(Component)
    {
        REFLECTION_BODY(Component)
      public:
        Component() = default;
        virtual ~Component() = default;
        // 在 AddComponent 调用
        void SetOwner(GameObject * owner)
        {
            _owner = owner;
        }

        GameObject* GetOwner() const
        {
            return _owner;
        }

        const std::string& GetReflectedClassName() const
        {
            return _reflectedClassName;
        }
        void SetReflectedClassName(const char* name)
        {
            _reflectedClassName = name;
        }

        // 当属性被修改的时候 进行一个调用
        virtual void OnPropertyChanged() {}

        virtual void Awake() {}
        // 修改 start 防止重复执行
        virtual void Start()
        {
            _isStarted = true;
        }
        virtual void Update(float deltaTime) {}
        virtual void FixedUpdate(float fixedDeltaTime) {}
        virtual void OnEnable() {}
        virtual void OnDisable() {}
        virtual void OnDestroy() {}

        bool IsEnabled() const
        {
            return _isEnabled;
        }
        // 仅设置 flag 调用逻辑交给 GO 实现
        // isEnabled 是告诉 GO 是否要执行 compment 逻辑的, 所以并不需要自身进行管理
        void SetEnabled(bool enabled)
        {
            _isEnabled = enabled;
        }

      private:
        GameObject* _owner = nullptr;
        bool _isEnabled = true;
        bool _isStarted = false;
        std::string _reflectedClassName = "Component";
    };
} // namespace ChikaEngine::Framework