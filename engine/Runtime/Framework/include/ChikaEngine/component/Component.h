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

        friend class GameObject;

      public:
        Component() = default;
        virtual ~Component() = default;

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

        virtual void Awake() {}
        virtual void Start() {}
        virtual void FixedTick(float fixedDeltaTime) {}
        virtual void Tick(float deltaTime) {}
        virtual void LateTick(float deltaTime) {}
        virtual void OnGizmo() const {}
        virtual void OnDirty() {}
        virtual void OnValidate() {}
        virtual void OnEnable() {}
        virtual void OnDisable() {}
        virtual void OnDestroy() {}

        bool IsEnabled() const
        {
            return _isEnabled;
        }
        bool IsActiveAndEnabled() const
        {
            return _isActiveAndEnabled;
        }

        void SetEnabled(bool enabled);

        void MarkDirty()
        {
            _isDirty = true;
            OnDirty();
            OnValidate();
        }

        bool IsStarted() const
        {
            return _isStarted;
        }

      protected:
        GameObject* _owner = nullptr;
        MFIELD()
        bool _isEnabled = true;
        bool _isStarted = false;
        bool _isAwake = false;
        bool _isActiveAndEnabled = false;
        bool _isDestroyed = false;
        MFIELD()
        std::string _reflectedClassName = "Component";

        bool _isDirty = false;
    };
} // namespace ChikaEngine::Framework
