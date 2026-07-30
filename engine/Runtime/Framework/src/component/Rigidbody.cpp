#include "ChikaEngine/component/Rigidbody.hpp"

#include "ChikaEngine/component/Collider.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.hpp"

#include <cmath>

namespace ChikaEngine::Framework
{
    Physics::MotionType Rigidbody::GetMotionType() const noexcept
    {
        if (_motionType < static_cast<int>(Physics::MotionType::Static) || _motionType > static_cast<int>(Physics::MotionType::Dynamic))
            return Physics::MotionType::Dynamic;
        return static_cast<Physics::MotionType>(_motionType);
    }

    void Rigidbody::SetMotionType(Physics::MotionType type)
    {
        _motionType = static_cast<int>(type);
        MarkDirty();
    }
    void Rigidbody::SetMass(float mass)
    {
        _mass = mass;
        MarkDirty();
    }
    void Rigidbody::SetLinearDamping(float damping)
    {
        _linearDamping = damping;
        MarkDirty();
    }
    void Rigidbody::SetAngularDamping(float damping)
    {
        _angularDamping = damping;
        MarkDirty();
    }
    void Rigidbody::SetGravityFactor(float factor)
    {
        _gravityFactor = factor;
        MarkDirty();
    }
    void Rigidbody::SetContinuousCollisionDetectionEnabled(bool enabled)
    {
        _continuousCollisionDetection = enabled;
        MarkDirty();
    }
    void Rigidbody::SetSleepingAllowed(bool allowed)
    {
        _allowSleeping = allowed;
        MarkDirty();
    }
    void Rigidbody::SetAxisLockMask(int mask)
    {
        _axisLockMask = mask;
        MarkDirty();
    }

    Scene* Rigidbody::GetSceneSafe() const
    {
        GameObject* owner = GetOwner();
        return owner ? owner->GetScene() : nullptr;
    }

    bool Rigidbody::ApplyAuthoringTo(Physics::PhysicsBodyCreateDesc& desc, std::string& diagnostic) const
    {
        if (_motionType < static_cast<int>(Physics::MotionType::Static) || _motionType > static_cast<int>(Physics::MotionType::Dynamic))
        {
            diagnostic = "Rigidbody motion type is invalid";
            return false;
        }
        if (!std::isfinite(_mass) || !std::isfinite(_linearDamping) || !std::isfinite(_angularDamping) || !std::isfinite(_gravityFactor) || _mass <= 0.0f || _linearDamping < 0.0f || _angularDamping < 0.0f)
        {
            diagnostic = "Rigidbody mass must be positive; damping and all dynamics values must be finite";
            return false;
        }
        if (_axisLockMask < Physics::PhysicsAxisLockNone || _axisLockMask > Physics::PhysicsAxisLockAll)
        {
            diagnostic = "Rigidbody axis lock mask is invalid";
            return false;
        }
        if (GetMotionType() != Physics::MotionType::Static && _axisLockMask == Physics::PhysicsAxisLockAll)
        {
            diagnostic = "Rigidbody cannot lock all degrees of freedom; use Static motion instead";
            return false;
        }

        desc.motionType = GetMotionType();
        desc.mass = _mass;
        desc.linearDamping = _linearDamping;
        desc.angularDamping = _angularDamping;
        desc.gravityFactor = _gravityFactor;
        desc.continuousCollisionDetection = _continuousCollisionDetection;
        desc.allowSleeping = _allowSleeping;
        desc.axisLockMask = static_cast<std::uint8_t>(_axisLockMask);
        return true;
    }

    void Rigidbody::RequestColliderRebuild()
    {
        GameObject* owner = GetOwner();
        Collider* collider = owner ? owner->GetComponent<Collider>() : nullptr;
        if (!collider || !collider->IsActiveAndEnabled())
        {
            _authoringDiagnostic = "Rigidbody requires an active Collider on the same GameObject";
            _physicsHandle = Physics::PhysicsBodyHandle::Invalid();
            return;
        }

        Physics::PhysicsBodyCreateDesc ignored;
        std::string diagnostic;
        if (!ApplyAuthoringTo(ignored, diagnostic))
        {
            _authoringDiagnostic = std::move(diagnostic);
            return;
        }
        _authoringDiagnostic.clear();
        collider->RequestBodyRebuild();
    }

    void Rigidbody::RefreshPhysicsHandle()
    {
        GameObject* owner = GetOwner();
        Scene* scene = GetSceneSafe();
        Physics::PhysicsScene* physics = scene ? scene->GetPhysicsSubsystem() : nullptr;
        _physicsHandle = owner && physics ? physics->GetBodyHandle(owner->GetID()) : Physics::PhysicsBodyHandle::Invalid();
    }

    void Rigidbody::Awake()
    {
        OnValidate();
    }
    void Rigidbody::Start()
    {
        RequestColliderRebuild();
    }
    void Rigidbody::FixedTick(float)
    {
        RefreshPhysicsHandle();
    }
    void Rigidbody::OnDirty()
    {
        RequestColliderRebuild();
    }
    void Rigidbody::OnEnable()
    {
        RequestColliderRebuild();
    }

    void Rigidbody::OnDisable()
    {
        _physicsHandle = Physics::PhysicsBodyHandle::Invalid();
        if (GameObject* owner = GetOwner())
        {
            if (Collider* collider = owner->GetComponent<Collider>(); collider && collider->IsActiveAndEnabled())
                collider->RequestBodyRebuild();
        }
    }

    void Rigidbody::OnDestroy()
    {
        _physicsHandle = Physics::PhysicsBodyHandle::Invalid();
        if (GameObject* owner = GetOwner())
        {
            if (Collider* collider = owner->GetComponent<Collider>(); collider && collider->IsActiveAndEnabled())
                collider->RequestBodyRebuild();
        }
    }

    void Rigidbody::OnValidate()
    {
        GameObject* owner = GetOwner();
        if (!owner || !owner->GetComponent<Collider>())
        {
            _authoringDiagnostic = "Rigidbody requires a Collider on the same GameObject";
            return;
        }
        Physics::PhysicsBodyCreateDesc ignored;
        std::string diagnostic;
        _authoringDiagnostic = ApplyAuthoringTo(ignored, diagnostic) ? std::string{} : std::move(diagnostic);
    }

    void Rigidbody::SetLinearVelocity(Math::Vector3 velocity)
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (scene && owner && scene->GetPhysicsSubsystem())
            (void)scene->GetPhysicsSubsystem()->QueueSetLinearVelocity(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), velocity);
    }

    void Rigidbody::SetAngularVelocity(Math::Vector3 velocity)
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (scene && owner && scene->GetPhysicsSubsystem())
            (void)scene->GetPhysicsSubsystem()->QueueSetAngularVelocity(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), velocity);
    }

    Math::Vector3 Rigidbody::GetLinearVelocity() const
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        const auto snapshot = scene && owner && scene->GetPhysicsSubsystem() ? scene->GetPhysicsSubsystem()->GetBodySnapshot(owner->GetID()) : std::nullopt;
        return snapshot ? snapshot->linearVelocity : Math::Vector3::zero;
    }

    Math::Vector3 Rigidbody::GetAngularVelocity() const
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        const auto snapshot = scene && owner && scene->GetPhysicsSubsystem() ? scene->GetPhysicsSubsystem()->GetBodySnapshot(owner->GetID()) : std::nullopt;
        return snapshot ? snapshot->angularVelocity : Math::Vector3::zero;
    }

    void Rigidbody::AddForce(Math::Vector3 force)
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (scene && owner && scene->GetPhysicsSubsystem())
            (void)scene->GetPhysicsSubsystem()->QueueAddForce(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), force);
    }

    void Rigidbody::AddTorque(Math::Vector3 torque)
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (scene && owner && scene->GetPhysicsSubsystem())
            (void)scene->GetPhysicsSubsystem()->QueueAddTorque(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), torque);
    }

    void Rigidbody::AddImpulse(Math::Vector3 impulse)
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (scene && owner && scene->GetPhysicsSubsystem())
            (void)scene->GetPhysicsSubsystem()->QueueApplyImpulse(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), impulse);
    }

    void Rigidbody::AddAngularImpulse(Math::Vector3 impulse)
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (scene && owner && scene->GetPhysicsSubsystem())
            (void)scene->GetPhysicsSubsystem()->QueueApplyAngularImpulse(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), impulse);
    }

    void Rigidbody::Impulse(Math::Vector3 impulse)
    {
        AddImpulse(impulse);
    }

    bool Rigidbody::MoveKinematic(const Math::Vector3& position, const Math::Quaternion& rotation)
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (GetMotionType() != Physics::MotionType::Kinematic || !scene || !owner || !owner->transform || !scene->GetPhysicsSubsystem())
            return false;
        const Physics::PhysicsResult result = scene->GetPhysicsSubsystem()->QueueKinematicTarget(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), position, rotation);
        if (result)
            owner->transform->SetWorldPositionAndRotation(position, rotation);
        return static_cast<bool>(result);
    }

    bool Rigidbody::Teleport(const Math::Vector3& position, const Math::Quaternion& rotation, bool resetVelocity, Physics::PhysicsWakePolicy wakePolicy)
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (GetMotionType() != Physics::MotionType::Dynamic || !scene || !owner || !scene->GetPhysicsSubsystem())
            return false;
        return static_cast<bool>(scene->GetPhysicsSubsystem()->QueueTeleport(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), position, rotation, resetVelocity, wakePolicy));
    }

    void Rigidbody::WakeUp()
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (scene && owner && scene->GetPhysicsSubsystem())
            (void)scene->GetPhysicsSubsystem()->QueueSetBodyActive(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), true);
    }

    void Rigidbody::Sleep()
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        if (scene && owner && scene->GetPhysicsSubsystem())
            (void)scene->GetPhysicsSubsystem()->QueueSetBodyActive(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), false);
    }

    bool Rigidbody::IsSleeping() const
    {
        Scene* scene = GetSceneSafe();
        GameObject* owner = GetOwner();
        const auto snapshot = scene && owner && scene->GetPhysicsSubsystem() ? scene->GetPhysicsSubsystem()->GetBodySnapshot(owner->GetID()) : std::nullopt;
        return snapshot && snapshot->sleeping;
    }
} // namespace ChikaEngine::Framework
