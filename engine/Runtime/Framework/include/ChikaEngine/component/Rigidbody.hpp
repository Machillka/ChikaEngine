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
        ~Rigidbody() override;
        void SetLinearVelocity(Math::Vector3 v);
        void Impulse(Math::Vector3 impulse);

        float GetColliderRadius() const
        {
            return _colliderRadius;
        }
        float GetColliderHeight() const
        {
            return _colliderHeight;
        }
        float GetMass() const
        {
            return _mass;
        }
        float GetFriction() const
        {
            return _friction;
        }

        void Awake() override;
        void OnGizmo() const override;
        void OnDirty() override;
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;

      private:
        void CreateRigidbody();
        Scene* GetSceneSave();

      private:
        Physics::PhysicsBodyHandle _physicsHandle = 0;
        MFIELD()
        Math::Vector3 _colliderCenter = Math::Vector3::zero;
        MFIELD()
        Math::Vector3 _colliderOffset = Math::Vector3::zero;
        MFIELD()
        float _colliderRadius = 0.5f;
        MFIELD()
        float _colliderHeight = 1.0f;
        MFIELD()
        float _mass = 1.0f;
        MFIELD()
        float _friction = 0.5f;

        bool _need2RecreateRigidbody = true;
    };
} // namespace ChikaEngine::Framework
