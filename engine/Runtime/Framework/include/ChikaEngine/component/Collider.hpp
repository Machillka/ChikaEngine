#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsHandles.hpp"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"

#include <string>

namespace ChikaEngine::Framework
{
    class Rigidbody;
    class Scene;

    /** @brief Geometry and collision authoring for one GameObject-owned physics body. */
    MCLASS(Collider) : public Component
    {
        REFLECTION_BODY(Collider)

      public:
        Collider() = default;
        ~Collider() override = default;

        [[nodiscard]] Physics::ColliderShapeType GetShapeType() const noexcept;
        [[nodiscard]] const Math::Vector3& GetCenter() const noexcept
        {
            return _center;
        }
        [[nodiscard]] const Math::Vector3& GetHalfExtents() const noexcept
        {
            return _halfExtents;
        }
        [[nodiscard]] float GetRadius() const noexcept
        {
            return _radius;
        }
        [[nodiscard]] float GetHeight() const noexcept
        {
            return _height;
        }
        [[nodiscard]] bool IsTrigger() const noexcept
        {
            return _isTrigger;
        }
        [[nodiscard]] int GetLayer() const noexcept
        {
            return _layer;
        }
        [[nodiscard]] float GetFriction() const noexcept
        {
            return _friction;
        }
        [[nodiscard]] float GetRestitution() const noexcept
        {
            return _restitution;
        }
        [[nodiscard]] bool IsQueryEnabled() const noexcept
        {
            return _queryEnabled;
        }
        [[nodiscard]] const std::string& GetCollisionProfile() const noexcept
        {
            return _collisionProfile;
        }
        [[nodiscard]] const std::string& GetMaterialName() const noexcept
        {
            return _materialName;
        }
        [[nodiscard]] Physics::PhysicsBodyHandle GetPhysicsHandle() const noexcept
        {
            return _physicsHandle;
        }
        [[nodiscard]] Physics::PhysicsColliderHandle GetColliderHandle() const noexcept
        {
            return _colliderHandle;
        }
        [[nodiscard]] const std::string& GetAuthoringDiagnostic() const noexcept
        {
            return _authoringDiagnostic;
        }

        void SetShapeType(Physics::ColliderShapeType type);
        void SetCenter(const Math::Vector3& center);
        void SetHalfExtents(const Math::Vector3& halfExtents);
        void SetRadius(float radius);
        void SetHeight(float height);
        void SetTrigger(bool trigger);
        void SetLayer(int layer);
        void SetFriction(float friction);
        void SetRestitution(float restitution);
        void SetQueryEnabled(bool enabled);
        void SetCollisionProfile(std::string profile);
        void SetMaterialName(std::string materialName);

        /** @brief Returns the exact scaled shape descriptor used by backend creation and gizmo drawing. */
        [[nodiscard]] Physics::ColliderShapeDesc BuildWorldShapeDesc() const;
        void RequestBodyRebuild();

        void Awake() override;
        void Start() override;
        void FixedTick(float fixedDeltaTime) override;
        void OnGizmo() const override;
        void OnDirty() override;
        void OnValidate() override;
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;

      private:
        friend class GameObject;

        void ApplyLegacyRigidbodyAuthoring(const Math::Vector3& center, float radius, float height, float friction);
        [[nodiscard]] bool BuildBodyCreateDesc(Physics::PhysicsBodyCreateDesc & desc, std::string & diagnostic) const;
        [[nodiscard]] bool QueueBodyCreateOrRebuild();
        [[nodiscard]] bool IsPrimaryCollider() const;
        void QueueBodyDestroy();
        void RefreshRuntimeHandles();
        [[nodiscard]] Scene* GetSceneSafe() const;

        Physics::PhysicsBodyHandle _physicsHandle = Physics::PhysicsBodyHandle::Invalid();
        Physics::PhysicsColliderHandle _colliderHandle = Physics::PhysicsColliderHandle::Invalid();

        // Reflection serializes only authoring data. Runtime handles and diagnostics remain transient.
        MFIELD()
        int _shapeType = static_cast<int>(Physics::ColliderShapeType::Box);
        MFIELD()
        Math::Vector3 _center = Math::Vector3::zero;
        MFIELD()
        Math::Vector3 _halfExtents{ 0.5f, 0.5f, 0.5f };
        MFIELD()
        float _radius = 0.5f;
        MFIELD()
        float _height = 1.0f;
        MFIELD()
        bool _isTrigger = false;
        MFIELD()
        int _layer = 0;
        MFIELD()
        float _friction = 0.5f;
        MFIELD()
        float _restitution = 0.0f;
        MFIELD()
        bool _queryEnabled = true;
        MFIELD()
        std::string _collisionProfile = "Default";
        MFIELD()
        std::string _materialName = "Default";

        bool _needsRebuild = true;
        std::string _authoringDiagnostic;
    };
} // namespace ChikaEngine::Framework
