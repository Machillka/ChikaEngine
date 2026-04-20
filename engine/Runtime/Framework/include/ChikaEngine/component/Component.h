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
        virtual void Tick(float deltaTime) {}
        virtual void OnGizmo() const {}
        virtual void OnDirty() {}
        virtual void OnEnable() {}
        virtual void OnDisable() {}

        bool IsEnabled() const
        {
            return _isEnabled;
        }

        void SetEnabled(bool enabled)
        {
            _isEnabled = enabled;
        }

        void MarkDirty()
        {
            _isDirty = true;
        }

      protected:
        GameObject* _owner = nullptr;
        MFIELD()
        bool _isEnabled = true;
        bool _isStarted = false;
        MFIELD()
        std::string _reflectedClassName = "Component";

        bool _isDirty;
    };
} // namespace ChikaEngine::Framework