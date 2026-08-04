#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsHandles.hpp"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"

#include <string>

namespace ChikaEngine::Framework
{
    class Collider;
    class Scene;

    /** @brief Motion and dynamics authoring layered on top of a Collider-owned body. */
    MCLASS(Rigidbody) : public Component
    {
        REFLECTION_BODY(Rigidbody);

      public:
        Rigidbody() = default;
        ~Rigidbody() override = default;

        void SetLinearVelocity(Math::Vector3 velocity);
        void SetAngularVelocity(Math::Vector3 velocity);
        [[nodiscard]] Math::Vector3 GetLinearVelocity() const;
        [[nodiscard]] Math::Vector3 GetAngularVelocity() const;
        /** @brief Adds force in newtons (kg*m/s^2) for the next fixed step. */
        void AddForce(Math::Vector3 force);
        /** @brief Adds torque in newton-metres for the next fixed step. */
        void AddTorque(Math::Vector3 torque);
        /** @brief Adds an instantaneous linear impulse in newton-seconds. */
        void AddImpulse(Math::Vector3 impulse);
        /** @brief Adds an instantaneous angular impulse in kg*m^2/s. */
        void AddAngularImpulse(Math::Vector3 impulse);
        /** @deprecated Use AddImpulse. */
        void Impulse(Math::Vector3 impulse);
        /** @brief Moves a Kinematic body to a world-space target during the next fixed step. */
        [[nodiscard]] bool MoveKinematic(const Math::Vector3& position, const Math::Quaternion& rotation);
        /** @brief Teleports a Dynamic body; resetVelocity and wake policy are explicit. */
        [[nodiscard]] bool Teleport(const Math::Vector3& position, const Math::Quaternion& rotation, bool resetVelocity = true, Physics::PhysicsWakePolicy wakePolicy = Physics::PhysicsWakePolicy::Wake);
        void WakeUp();
        void Sleep();
        [[nodiscard]] bool IsSleeping() const;

        [[nodiscard]] Physics::PhysicsBodyHandle GetPhysicsHandle() const noexcept
        {
            return _physicsHandle;
        }
        [[nodiscard]] Physics::MotionType GetMotionType() const noexcept;
        [[nodiscard]] float GetMass() const noexcept
        {
            return _mass;
        }
        [[nodiscard]] float GetLinearDamping() const noexcept
        {
            return _linearDamping;
        }
        [[nodiscard]] float GetAngularDamping() const noexcept
        {
            return _angularDamping;
        }
        [[nodiscard]] float GetGravityFactor() const noexcept
        {
            return _gravityFactor;
        }
        [[nodiscard]] bool IsContinuousCollisionDetectionEnabled() const noexcept
        {
            return _continuousCollisionDetection;
        }
        [[nodiscard]] bool IsSleepingAllowed() const noexcept
        {
            return _allowSleeping;
        }
        [[nodiscard]] int GetAxisLockMask() const noexcept
        {
            return _axisLockMask;
        }
        [[nodiscard]] const std::string& GetAuthoringDiagnostic() const noexcept
        {
            return _authoringDiagnostic;
        }

        void SetMotionType(Physics::MotionType type);
        void SetMass(float mass);
        void SetLinearDamping(float damping);
        void SetAngularDamping(float damping);
        void SetGravityFactor(float factor);
        void SetContinuousCollisionDetectionEnabled(bool enabled);
        void SetSleepingAllowed(bool allowed);
        void SetAxisLockMask(int mask);

        void Awake() override;
        void Start() override;
        void FixedTick(float fixedDeltaTime) override;
        void OnDirty() override;
        void OnValidate() override;
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;

      private:
        friend class Collider;

        [[nodiscard]] bool ApplyAuthoringTo(Physics::PhysicsBodyCreateDesc & desc, std::string & diagnostic) const;
        void RequestColliderRebuild();
        void RefreshPhysicsHandle();
        [[nodiscard]] Scene* GetSceneSafe() const;

        Physics::PhysicsBodyHandle _physicsHandle = Physics::PhysicsBodyHandle::Invalid();

        MFIELD()
        int _motionType = static_cast<int>(Physics::MotionType::Dynamic);
        MFIELD()
        float _mass = 1.0f;
        MFIELD()
        float _linearDamping = 0.05f;
        MFIELD()
        float _angularDamping = 0.05f;
        MFIELD()
        float _gravityFactor = 1.0f;
        MFIELD()
        bool _continuousCollisionDetection = false;
        MFIELD()
        bool _allowSleeping = true;
        MFIELD()
        int _axisLockMask = Physics::PhysicsAxisLockNone;

        std::string _authoringDiagnostic;
    };
} // namespace ChikaEngine::Framework
