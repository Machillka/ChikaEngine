#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"
namespace ChikaEngine::Framework
{
    class Scene;
    class GameObject;

    MCLASS(Rigidbody) : public Component
    {
        REFLECTION_BODY(Rigidbody);

      public:
        Rigidbody() = default;
        ~Rigidbody() = default;
        void SetLinearVelocity(Math::Vector3 v);
        void Impulse(Math::Vector3 impulse);

        void Awake() override;
        void OnGizmo() const override;
        void OnDirty() override;
        void OnEnable() override;
        void OnDisable() override;

      private:
        void CreateRigidbody();
        Scene* GetSceneSave();

      private:
        Physics::PhysicsBodyHandle _physicsHandle;
        MFIELD()
        Math::Vector3 _colliderCenter;
        MFIELD()
        Math::Vector3 _colliderOffset;
        MFIELD()
        float _colliderRadius;
        MFIELD()
        float _colliderHeight;
        MFIELD()
        float _mass;
        MFIELD()
        float _friction;

        bool _need2RecreateRigidbody = true;
    };
} // namespace ChikaEngine::Framework