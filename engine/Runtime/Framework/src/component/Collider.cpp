#include "ChikaEngine/component/Collider.hpp"

#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/debug/Gizmo.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ChikaEngine::Framework
{
    namespace
    {
        bool IsFinite(const Math::Vector3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        Math::Vector3 Abs(const Math::Vector3& value)
        {
            return { std::abs(value.x), std::abs(value.y), std::abs(value.z) };
        }

        bool IsUniformScale(const Math::Vector3& value)
        {
            const Math::Vector3 scale = Abs(value);
            constexpr float epsilon = 1.0e-5f;
            return std::abs(scale.x - scale.y) <= epsilon && std::abs(scale.y - scale.z) <= epsilon;
        }
    } // namespace

    Physics::ColliderShapeType Collider::GetShapeType() const noexcept
    {
        if (_shapeType < static_cast<int>(Physics::ColliderShapeType::Box) || _shapeType > static_cast<int>(Physics::ColliderShapeType::Capsule))
            return Physics::ColliderShapeType::Box;
        return static_cast<Physics::ColliderShapeType>(_shapeType);
    }

    void Collider::SetShapeType(Physics::ColliderShapeType type)
    {
        _shapeType = static_cast<int>(type);
        MarkDirty();
    }
    void Collider::SetCenter(const Math::Vector3& center)
    {
        _center = center;
        MarkDirty();
    }
    void Collider::SetHalfExtents(const Math::Vector3& halfExtents)
    {
        _halfExtents = halfExtents;
        MarkDirty();
    }
    void Collider::SetRadius(float radius)
    {
        _radius = radius;
        MarkDirty();
    }
    void Collider::SetHeight(float height)
    {
        _height = height;
        MarkDirty();
    }
    void Collider::SetTrigger(bool trigger)
    {
        _isTrigger = trigger;
        MarkDirty();
    }
    void Collider::SetLayer(int layer)
    {
        _layer = layer;
        MarkDirty();
    }
    void Collider::SetFriction(float friction)
    {
        _friction = friction;
        MarkDirty();
    }
    void Collider::SetRestitution(float restitution)
    {
        _restitution = restitution;
        MarkDirty();
    }
    void Collider::SetQueryEnabled(bool enabled)
    {
        _queryEnabled = enabled;
        MarkDirty();
    }
    void Collider::SetCollisionProfile(std::string profile)
    {
        _collisionProfile = std::move(profile);
        MarkDirty();
    }
    void Collider::SetMaterialName(std::string materialName)
    {
        _materialName = std::move(materialName);
        MarkDirty();
    }

    Scene* Collider::GetSceneSafe() const
    {
        GameObject* owner = GetOwner();
        return owner ? owner->GetScene() : nullptr;
    }

    bool Collider::IsPrimaryCollider() const
    {
        const GameObject* owner = GetOwner();
        return owner && owner->GetComponent<Collider>() == this;
    }

    Physics::ColliderShapeDesc Collider::BuildWorldShapeDesc() const
    {
        Physics::ColliderShapeDesc result;
        result.type = GetShapeType();

        const GameObject* owner = GetOwner();
        const Math::Vector3 scale = owner && owner->transform ? owner->transform->GetWorldScale() : Math::Vector3{ 1.0f, 1.0f, 1.0f };
        const Math::Vector3 absoluteScale = Abs(scale);
        result.center = _center * scale;
        result.halfExtents = _halfExtents * absoluteScale;
        result.radius = _radius * std::max({ absoluteScale.x, absoluteScale.y, absoluteScale.z });
        result.height = _height * absoluteScale.y;
        if (result.type == Physics::ColliderShapeType::Capsule)
            result.radius = _radius * std::max(absoluteScale.x, absoluteScale.z);
        return result;
    }

    bool Collider::BuildBodyCreateDesc(Physics::PhysicsBodyCreateDesc& desc, std::string& diagnostic) const
    {
        GameObject* owner = GetOwner();
        if (!owner || !owner->transform)
        {
            diagnostic = "Collider requires an owning GameObject with Transform";
            return false;
        }
        if (!IsPrimaryCollider())
        {
            diagnostic = "Only one Collider per GameObject is supported in Step 2.1";
            return false;
        }
        if (owner->transform->GetParent() && !IsUniformScale(owner->transform->GetParent()->GetWorldScale()))
        {
            diagnostic = "Parented physics bodies require uniform parent world scale";
            return false;
        }
        if (_shapeType < static_cast<int>(Physics::ColliderShapeType::Box) || _shapeType > static_cast<int>(Physics::ColliderShapeType::Capsule))
        {
            diagnostic = "Collider shape value is invalid";
            return false;
        }
        if (!IsFinite(_center) || !IsFinite(_halfExtents) || !std::isfinite(_radius) || !std::isfinite(_height) || !std::isfinite(_friction) || !std::isfinite(_restitution))
        {
            diagnostic = "Collider values must be finite";
            return false;
        }
        if (_layer < 0 || _layer >= Physics::PHYSICS_LAYER_COUNT)
        {
            diagnostic = "Collider layer must be in [0, 31]";
            return false;
        }
        if (_friction < 0.0f || _restitution < 0.0f || _restitution > 1.0f)
        {
            diagnostic = "Collider friction must be non-negative and restitution must be in [0, 1]";
            return false;
        }

        const Physics::ColliderShapeDesc shape = BuildWorldShapeDesc();
        if (shape.type == Physics::ColliderShapeType::Box && (shape.halfExtents.x <= 0.0f || shape.halfExtents.y <= 0.0f || shape.halfExtents.z <= 0.0f))
        {
            diagnostic = "Box Collider half extents must be positive after world scale";
            return false;
        }
        if ((shape.type == Physics::ColliderShapeType::Sphere || shape.type == Physics::ColliderShapeType::Capsule) && shape.radius <= 0.0f)
        {
            diagnostic = "Sphere/Capsule Collider radius must be positive after world scale";
            return false;
        }
        if (shape.type == Physics::ColliderShapeType::Capsule && shape.height <= 0.0f)
        {
            diagnostic = "Capsule Collider height must be positive after world scale";
            return false;
        }

        Scene* scene = owner->GetScene();
        Physics::PhysicsScene* physics = scene ? scene->GetPhysicsSubsystem() : nullptr;
        if (physics && !physics->GetCapabilities().SupportsShape(shape.type))
        {
            diagnostic = "Selected Collider shape is unsupported by the active physics backend";
            return false;
        }

        desc.ownerId = owner->GetID();
        desc.position = owner->transform->GetWorldPosition();
        desc.rotation = owner->transform->GetWorldRotation();
        desc.shapeDesc = shape;
        desc.isTrigger = _isTrigger;
        desc.layer = static_cast<Physics::PhysicsLayerID>(_layer);
        desc.friction = _friction;
        desc.restitution = _restitution;
        desc.queryEnabled = _queryEnabled;
        desc.motionType = Physics::MotionType::Static;

        if (Rigidbody* rigidbody = owner->GetComponent<Rigidbody>(); rigidbody && rigidbody->IsActiveAndEnabled())
            return rigidbody->ApplyAuthoringTo(desc, diagnostic);
        return true;
    }

    bool Collider::QueueBodyCreateOrRebuild()
    {
        GameObject* owner = GetOwner();
        Scene* scene = GetSceneSafe();
        Physics::PhysicsScene* physics = scene ? scene->GetPhysicsSubsystem() : nullptr;
        if (!owner || !physics)
        {
            _authoringDiagnostic = "Collider owner Scene does not have an initialized physics subsystem";
            return false;
        }

        Physics::PhysicsBodyCreateDesc desc;
        if (!BuildBodyCreateDesc(desc, _authoringDiagnostic))
        {
            LOG_ERROR("Collider", "Cannot build Collider for owner {}: {}", owner->GetID(), _authoringDiagnostic);
            return false;
        }

        _authoringDiagnostic.clear();
        const Physics::PhysicsResult result = physics->GetBodyHandle(owner->GetID()) ? physics->QueueRebuildBody(desc) : physics->QueueCreateBody(desc);
        if (!result)
        {
            _authoringDiagnostic = result.diagnostic;
            LOG_ERROR("Collider", "Failed to queue body lifecycle command: {}", result.diagnostic);
            return false;
        }
        return true;
    }

    void Collider::QueueBodyDestroy()
    {
        if (!IsPrimaryCollider())
            return;
        GameObject* owner = GetOwner();
        Scene* scene = GetSceneSafe();
        if (owner && scene && scene->GetPhysicsSubsystem() && (scene->IsPlaying() || scene->IsPaused()))
            (void)scene->GetPhysicsSubsystem()->QueueDestroyBody(owner->GetID());
        _physicsHandle = Physics::PhysicsBodyHandle::Invalid();
        _colliderHandle = Physics::PhysicsColliderHandle::Invalid();
    }

    void Collider::RefreshRuntimeHandles()
    {
        GameObject* owner = GetOwner();
        Scene* scene = GetSceneSafe();
        Physics::PhysicsScene* physics = scene ? scene->GetPhysicsSubsystem() : nullptr;
        if (!owner || !physics)
        {
            _physicsHandle = Physics::PhysicsBodyHandle::Invalid();
            _colliderHandle = Physics::PhysicsColliderHandle::Invalid();
            return;
        }
        _physicsHandle = physics->GetBodyHandle(owner->GetID());
        const auto record = physics->GetBodyRecord(_physicsHandle);
        _colliderHandle = record ? record->colliderHandle : Physics::PhysicsColliderHandle::Invalid();
    }

    void Collider::RequestBodyRebuild()
    {
        _needsRebuild = true;
        Scene* scene = GetSceneSafe();
        if (IsActiveAndEnabled() && scene && (scene->IsPlaying() || scene->IsPaused()))
            _needsRebuild = !QueueBodyCreateOrRebuild();
    }

    void Collider::Awake()
    {
        _needsRebuild = true;
        if (GameObject* owner = GetOwner())
        {
            if (Rigidbody* rigidbody = owner->GetComponent<Rigidbody>())
                rigidbody->OnValidate();
        }
    }

    void Collider::Start()
    {
        if (_needsRebuild)
            _needsRebuild = !QueueBodyCreateOrRebuild();
    }

    void Collider::FixedTick(float)
    {
        RefreshRuntimeHandles();
    }

    void Collider::OnDirty()
    {
        RequestBodyRebuild();
    }

    void Collider::OnValidate()
    {
        Physics::PhysicsBodyCreateDesc ignored;
        std::string diagnostic;
        if (!BuildBodyCreateDesc(ignored, diagnostic))
            _authoringDiagnostic = std::move(diagnostic);
        else
            _authoringDiagnostic.clear();
    }

    void Collider::OnEnable()
    {
        if (GameObject* owner = GetOwner())
        {
            if (Rigidbody* rigidbody = owner->GetComponent<Rigidbody>())
                rigidbody->OnValidate();
        }
        RequestBodyRebuild();
    }

    void Collider::OnDisable()
    {
        if (GameObject* owner = GetOwner())
        {
            if (Rigidbody* rigidbody = owner->GetComponent<Rigidbody>())
                rigidbody->_authoringDiagnostic = "Rigidbody requires an active Collider on the same GameObject";
        }
        QueueBodyDestroy();
        _needsRebuild = true;
    }

    void Collider::OnDestroy()
    {
        if (GameObject* owner = GetOwner())
        {
            if (Rigidbody* rigidbody = owner->GetComponent<Rigidbody>())
                rigidbody->_authoringDiagnostic = "Rigidbody requires a Collider on the same GameObject";
        }
        QueueBodyDestroy();
    }

    void Collider::OnGizmo() const
    {
        const GameObject* owner = GetOwner();
        if (!owner || !owner->transform)
            return;

        const Physics::ColliderShapeDesc shape = BuildWorldShapeDesc();
        const Math::Quaternion rotation = owner->transform->GetWorldRotation();
        const Math::Vector3 center = owner->transform->GetWorldPosition() + rotation.Rotate(shape.center);
        constexpr Math::Vector4 color{ 0.2f, 1.0f, 0.2f, 1.0f };
        switch (shape.type)
        {
        case Physics::ColliderShapeType::Box:
            Debug::Gizmo::DrawWireBox(center, shape.halfExtents, rotation, color);
            break;
        case Physics::ColliderShapeType::Sphere:
            Debug::Gizmo::DrawWireSphere(center, shape.radius, rotation, color);
            break;
        case Physics::ColliderShapeType::Capsule:
            Debug::Gizmo::DrawWireCapsule(center, shape.radius, shape.height, rotation, color);
            break;
        }
    }

    void Collider::ApplyLegacyRigidbodyAuthoring(const Math::Vector3& center, float radius, float height, float friction)
    {
        _shapeType = static_cast<int>(Physics::ColliderShapeType::Box);
        _center = center;
        _halfExtents = { 0.5f, 0.5f, 0.5f };
        _radius = radius;
        _height = height;
        _friction = friction;
        _needsRebuild = true;
    }
} // namespace ChikaEngine::Framework
