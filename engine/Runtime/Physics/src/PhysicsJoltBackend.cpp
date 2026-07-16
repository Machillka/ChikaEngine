#include "PhysicsJoltBackend.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include "JoltLayer.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "Jolt/Core/Reference.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include "Jolt/Math/Quat.h"
#include "Jolt/Math/Real.h"
#include "Jolt/Physics/Body/BodyID.h"
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ChikaEngine::Physics
{
    namespace
    {
        bool IsFinite(const Math::Vector3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool IsFinite(const Math::Quaternion& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
        }

        bool IsNormalized(const Math::Quaternion& value)
        {
            const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w;
            return std::abs(lengthSquared - 1.0f) <= 1.0e-3f;
        }

        bool IsValidShape(const ColliderShapeDesc& desc)
        {
            if (!IsFinite(desc.center))
                return false;

            switch (desc.type)
            {
            case ColliderShapeType::Box:
                return IsFinite(desc.halfExtents) && desc.halfExtents.x > 0.0f && desc.halfExtents.y > 0.0f && desc.halfExtents.z > 0.0f;
            case ColliderShapeType::Sphere:
                return std::isfinite(desc.radius) && desc.radius > 0.0f;
            case ColliderShapeType::Capsule:
                return std::isfinite(desc.radius) && std::isfinite(desc.height) && desc.radius > 0.0f && desc.height > 0.0f;
            }
            return false;
        }

        JPH::EActivation ToJoltActivation(PhysicsWakePolicy wakePolicy, bool isActive) noexcept
        {
            switch (wakePolicy)
            {
            case PhysicsWakePolicy::Wake:
                return JPH::EActivation::Activate;
            case PhysicsWakePolicy::DoNotWake:
                return JPH::EActivation::DontActivate;
            case PhysicsWakePolicy::KeepState:
                return isActive ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
            }
            return JPH::EActivation::Activate;
        }
    } // namespace

    class PhysicsJoltBackend::JoltBackendContactListener final : public JPH::ContactListener
    {
      public:
        explicit JoltBackendContactListener(PhysicsJoltBackend* physicsBackend) : _physicsBackend(physicsBackend) {}

        void OnContactAdded(const JPH::Body& bodyA, const JPH::Body& bodyB, const JPH::ContactManifold& manifold, JPH::ContactSettings&) override
        {
            if (!_physicsBackend)
                return;

            const PhysicsBodyHandle handleA = PhysicsBodyHandle::FromValue(bodyA.GetUserData());
            const PhysicsBodyHandle handleB = PhysicsBodyHandle::FromValue(bodyB.GetUserData());
            if (!handleA || !handleB)
                return;

            const JPH::RVec3 point = manifold.GetWorldSpaceContactPointOn1(0);
            const JPH::Vec3 normal = manifold.mWorldSpaceNormal;

            CollisionEvent eventA;
            eventA.selfRigidbodyHandle = handleA;
            eventA.otherRigidbodyHandle = handleB;
            eventA.contactPoint = Math::Vector3(static_cast<float>(point.GetX()), static_cast<float>(point.GetY()), static_cast<float>(point.GetZ()));
            eventA.contactNormal = Math::Vector3(normal.GetX(), normal.GetY(), normal.GetZ());

            CollisionEvent eventB = eventA;
            eventB.selfRigidbodyHandle = handleB;
            eventB.otherRigidbodyHandle = handleA;
            eventB.contactNormal = Math::Vector3(-eventA.contactNormal.x, -eventA.contactNormal.y, -eventA.contactNormal.z);

            _physicsBackend->PushEvent(eventA);
            _physicsBackend->PushEvent(eventB);
        }

      private:
        PhysicsJoltBackend* _physicsBackend = nullptr;
    };

    PhysicsJoltBackend::PhysicsJoltBackend()
    {
        _masks.resize(PHYSICS_LAYER_COUNT, PHYSICS_LAYER_MASK_ALL);
    }

    PhysicsJoltBackend::~PhysicsJoltBackend()
    {
        Shutdown();
    }

    void PhysicsJoltBackend::PushEvent(const CollisionEvent& event)
    {
        std::lock_guard lock(_eventMutex);
        _eventQueue.push_back(event);
    }

    std::vector<CollisionEvent> PhysicsJoltBackend::PollCollisionEvents()
    {
        std::lock_guard lock(_eventMutex);
        std::vector<CollisionEvent> snapshot;
        snapshot.swap(_eventQueue);
        return snapshot;
    }

    JPH::Ref<JPH::Shape> PhysicsJoltBackend::CreateShape(const ColliderShapeDesc& desc)
    {
        switch (desc.type)
        {
        case ColliderShapeType::Box:
        {
            JPH::Vec3 half(desc.halfExtents.x, desc.halfExtents.y, desc.halfExtents.z);
            JPH::BoxShapeSettings settings(half);
            JPH::BoxShapeSettings::ShapeResult res = settings.Create();
            if (res.HasError())
            {
                LOG_ERROR("Physics", "Unable to create rigidbody");
                return nullptr;
            }
            JPH::Ref<JPH::Shape> shape = res.Get();
            return shape;
        }

        case ColliderShapeType::Sphere:
        {
            JPH::SphereShapeSettings settings(desc.radius);
            JPH::SphereShapeSettings::ShapeResult res = settings.Create();
            if (res.HasError())
            {
                LOG_ERROR("Physics", "Unable to create rigidbody");
                return nullptr;
            }
            JPH::Ref<JPH::Shape> shape = res.Get();
            return shape;
        }
        case ColliderShapeType::Capsule:
            return nullptr;
        }
        return nullptr;
    }

    PhysicsResult PhysicsJoltBackend::Initialize(const PhysicsInitDesc& desc)
    {
        if (_initialized)
            return PhysicsResult{ .status = PhysicsStatus::AlreadyInitialized, .diagnostic = "Jolt backend is already initialized" };
        if (!IsFinite(desc.gravity) || desc.workerThreadCount < -1)
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics initialization contains invalid gravity or worker count");

        PhysicsResult runtimeResult = PhysicsRuntime::Acquire(_runtimeLease);
        if (!runtimeResult)
            return runtimeResult;

        try
        {
            _tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
            _jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, desc.workerThreadCount);
            _physicsSystem = std::make_unique<JPH::PhysicsSystem>();
            _bpInterface = std::make_unique<JoltHelper::BitmaskBroadPhaseLayerInterface>();
            _objVsBPFilter = std::make_unique<JoltHelper::BitmaskObjectVsBroadPhaseLayerFilter>();
            _pairFilter = std::make_unique<JoltHelper::BitmaskObjectLayerPairFilter>(this);

            constexpr std::uint32_t maxBodies = 2048;
            constexpr std::uint32_t bodyMutexCount = 0;
            constexpr std::uint32_t maxBodyPairs = 2048;
            constexpr std::uint32_t maxContactConstraints = 1024;
            _physicsSystem->Init(maxBodies, bodyMutexCount, maxBodyPairs, maxContactConstraints, *_bpInterface, *_objVsBPFilter, *_pairFilter);

            _bodyInterface = &_physicsSystem->GetBodyInterface();
            _physicsSystem->SetGravity(JPH::Vec3(desc.gravity.x, desc.gravity.y, desc.gravity.z));
            _listener = std::make_unique<JoltBackendContactListener>(this);
            _physicsSystem->SetContactListener(_listener.get());
            _initialized = true;
        }
        catch (const std::exception& exception)
        {
            const std::string diagnostic = std::string("Failed to initialize Jolt backend: ") + exception.what();
            Shutdown();
            return PhysicsResult::Failure(PhysicsStatus::BackendFailure, diagnostic);
        }
        catch (...)
        {
            Shutdown();
            return PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Failed to initialize Jolt backend");
        }

        LOG_INFO("JoltBackend", "Initialized with {} worker thread(s)", desc.workerThreadCount);
        return PhysicsResult::Ok();
    }

    void PhysicsJoltBackend::Shutdown() noexcept
    {
        const bool hadState = _initialized || _runtimeLease.IsActive() || _physicsSystem != nullptr;
        if (_physicsSystem && _listener)
            _physicsSystem->SetContactListener(nullptr);
        _listener.reset();
        DestroyAllBodies();
        _bodyInterface = nullptr;
        _physicsSystem.reset();
        _pairFilter.reset();
        _objVsBPFilter.reset();
        _bpInterface.reset();
        _jobSystem.reset();
        _tempAllocator.reset();

        {
            std::lock_guard lock(_eventMutex);
            _eventQueue.clear();
        }
        _initialized = false;
        _runtimeLease.Reset();
        if (hadState)
            LOG_INFO("JoltBackend", "Shutdown complete");
    }

    bool PhysicsJoltBackend::IsInitialized() const noexcept
    {
        return _initialized;
    }

    PhysicsBackendCapabilities PhysicsJoltBackend::GetCapabilities() const noexcept
    {
        return PhysicsBackendCapabilities{
            .boxShape = true,
            .sphereShape = true,
            .capsuleShape = false,
            .closestRaycast = true,
            .constraints = false,
            .continuousCollisionDetection = false,
        };
    }

    void PhysicsJoltBackend::ClearBodies() noexcept
    {
        DestroyAllBodies();
    }

    bool PhysicsJoltBackend::ResolveBodyId(PhysicsBackendBodyToken token, std::uint32_t& backendBodyId) const
    {
        if (!token || token.Value() > std::numeric_limits<std::uint32_t>::max())
            return false;
        backendBodyId = static_cast<std::uint32_t>(token.Value());
        return backendBodyId != JPH::BodyID::cInvalidBodyID && IsTrackedBodyId(backendBodyId);
    }

    bool PhysicsJoltBackend::IsTrackedBodyId(std::uint32_t backendBodyId) const
    {
        std::lock_guard lock(_bodySetMutex);
        return _bodyIds.contains(backendBodyId);
    }

    void PhysicsJoltBackend::DestroyAllBodies() noexcept
    {
        std::vector<std::uint32_t> bodies;
        {
            std::lock_guard lock(_bodySetMutex);
            bodies.assign(_bodyIds.begin(), _bodyIds.end());
        }

        if (_bodyInterface)
        {
            for (const std::uint32_t backendBodyId : bodies)
            {
                const JPH::BodyID id(backendBodyId);
                if (_bodyInterface->IsAdded(id))
                    _bodyInterface->RemoveBody(id);
                _bodyInterface->DestroyBody(id);
            }
        }

        {
            std::lock_guard lock(_bodySetMutex);
            _bodyIds.clear();
        }
    }

    bool PhysicsJoltBackend::SetLinearVelocity(PhysicsBackendBodyToken token, const Math::Vector3& velocity)
    {
        std::uint32_t backendBodyId = 0;
        if (!_initialized || !IsFinite(velocity) || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;
        _bodyInterface->SetLinearVelocity(id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
        return true;
    }

    bool PhysicsJoltBackend::AddForce(PhysicsBackendBodyToken token, const Math::Vector3& force, PhysicsWakePolicy wakePolicy)
    {
        std::uint32_t backendBodyId = 0;
        if (!_initialized || !IsFinite(force) || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;
        _bodyInterface->AddForce(id, JPH::Vec3(force.x, force.y, force.z), ToJoltActivation(wakePolicy, _bodyInterface->IsActive(id)));
        return true;
    }

    bool PhysicsJoltBackend::ApplyImpulse(PhysicsBackendBodyToken token, const Math::Vector3& impulse)
    {
        std::uint32_t backendBodyId = 0;
        if (!_initialized || !IsFinite(impulse) || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;
        _bodyInterface->AddImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
        return true;
    }

    bool PhysicsJoltBackend::TeleportBody(PhysicsBackendBodyToken token, const Math::Vector3& position, const Math::Quaternion& rotation, bool resetVelocity, PhysicsWakePolicy wakePolicy)
    {
        std::uint32_t backendBodyId = 0;
        if (!_initialized || !IsFinite(position) || !IsFinite(rotation) || !IsNormalized(rotation) || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;
        const bool wasActive = _bodyInterface->IsActive(id);
        _bodyInterface->SetPositionAndRotation(id, JPH::RVec3(position.x, position.y, position.z), JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w), ToJoltActivation(wakePolicy, wasActive));
        if (resetVelocity)
        {
            _bodyInterface->SetLinearVelocity(id, JPH::Vec3::sZero());
            _bodyInterface->SetAngularVelocity(id, JPH::Vec3::sZero());
        }
        if (wakePolicy == PhysicsWakePolicy::DoNotWake || (wakePolicy == PhysicsWakePolicy::KeepState && !wasActive))
            _bodyInterface->DeactivateBody(id);
        return true;
    }

    bool PhysicsJoltBackend::SetKinematicTarget(PhysicsBackendBodyToken token, const Math::Vector3& position, const Math::Quaternion& rotation, float deltaTime)
    {
        std::uint32_t backendBodyId = 0;
        if (!_initialized || !IsFinite(position) || !IsFinite(rotation) || !IsNormalized(rotation) || !std::isfinite(deltaTime) || deltaTime <= 0.0f || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;
        _bodyInterface->MoveKinematic(id, JPH::RVec3(position.x, position.y, position.z), JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w), deltaTime);
        return true;
    }

    bool PhysicsJoltBackend::Simulate(float fixedDeltaTime)
    {
        if (!_initialized || !_physicsSystem || !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
            return false;

        _physicsSystem->Update(fixedDeltaTime, 1, _tempAllocator.get(), _jobSystem.get());
        return true;
    }

    bool PhysicsJoltBackend::SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask)
    {
        if (layerId >= PHYSICS_LAYER_COUNT)
            return false;
        std::lock_guard lock(_maskMutex);
        _masks[layerId] = mask;
        return true;
    }

    PhysicsLayerMask PhysicsJoltBackend::GetLayerCollisionMask(PhysicsLayerID layerId) const
    {
        if (layerId >= PHYSICS_LAYER_COUNT)
            return 0;
        std::lock_guard lock(_maskMutex);
        return _masks[layerId];
    }

    PhysicsBackendBodyCreateResult PhysicsJoltBackend::CreateBodyFromDesc(PhysicsBodyHandle engineHandle, const PhysicsBodyCreateDesc& desc)
    {
        if (!_initialized || !_bodyInterface)
            return { .result = PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Jolt backend is not initialized") };
        if (!engineHandle)
            return { .result = PhysicsResult::Failure(PhysicsStatus::InvalidHandle, "Engine body handle is invalid") };
        if (desc.layer >= PHYSICS_LAYER_COUNT)
            return { .result = PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics body layer must be in [0, 31]") };
        if (!GetCapabilities().SupportsShape(desc.shapeDesc.type))
            return { .result = PhysicsResult::Failure(PhysicsStatus::UnsupportedFeature, "Requested collider shape is not implemented by the Jolt adapter") };
        if (!IsFinite(desc.position) || !IsFinite(desc.rotation) || !IsNormalized(desc.rotation) || !IsValidShape(desc.shapeDesc) || !std::isfinite(desc.mass) || !std::isfinite(desc.friction) || !std::isfinite(desc.restitution) || desc.mass <= 0.0f || desc.friction < 0.0f || desc.restitution < 0.0f || desc.restitution > 1.0f)
            return { .result = PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics body descriptor contains invalid numeric values") };

        using namespace JPH;
        JPH::Ref<JPH::Shape> shape = CreateShape(desc.shapeDesc);
        if (!shape)
            return { .result = PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Jolt failed to create collider shape") };

        RVec3 pos(desc.position.x, desc.position.y, desc.position.z);
        Quat rot(desc.rotation.x, desc.rotation.y, desc.rotation.z, desc.rotation.w);

        BodyCreationSettings settings;
        settings.mPosition = pos;
        settings.mRotation = rot;
        settings.SetShape(shape);
        settings.mUserData = engineHandle.Value();

        if (desc.motionType == MotionType::Static)
            settings.mMotionType = EMotionType::Static;
        else if (desc.motionType == MotionType::Kinematic)
            settings.mMotionType = EMotionType::Kinematic;
        else
            settings.mMotionType = EMotionType::Dynamic;

        // 计算编码后的 ObjectLayer
        settings.mObjectLayer = JoltHelper::GetJoltObjectLayer(desc.layer, desc.motionType);

        if (desc.motionType != MotionType::Static)
        {
            settings.mMassPropertiesOverride.mMass = desc.mass;
            settings.mAllowDynamicOrKinematic = true;

            // NOTE: 此处加上一点线形阻尼 防止乱飘
            settings.mLinearDamping = 0.05f;
            settings.mAngularDamping = 0.05f;
        }

        settings.mIsSensor = desc.isTrigger;
        settings.mFriction = desc.friction;
        settings.mRestitution = desc.restitution;
        BodyID id = _bodyInterface->CreateAndAddBody(settings, EActivation::Activate);
        if (id.IsInvalid())
            return { .result = PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Jolt failed to create physics body") };

        {
            std::lock_guard lock(_bodySetMutex);
            _bodyIds.insert(id.GetIndexAndSequenceNumber());
        }

        LOG_INFO("Physics", "Created body handle index={}, generation={}", engineHandle.Index(), engineHandle.Generation());
        return { .result = PhysicsResult::Ok(), .token = PhysicsBackendBodyToken::FromValue(id.GetIndexAndSequenceNumber()) };
    }

    bool PhysicsJoltBackend::DestroyPhysicsBody(PhysicsBackendBodyToken token)
    {
        std::uint32_t backendBodyId = 0;
        if (!_bodyInterface || !ResolveBodyId(token, backendBodyId))
            return false;

        const JPH::BodyID id(backendBodyId);
        if (_bodyInterface->IsAdded(id))
            _bodyInterface->RemoveBody(id);
        _bodyInterface->DestroyBody(id);
        std::lock_guard lock(_bodySetMutex);
        return _bodyIds.erase(backendBodyId) == 1;
    }

    bool PhysicsJoltBackend::TrySyncTransform(PhysicsBackendBodyToken token, PhysicsTransform& transform)
    {
        std::uint32_t backendBodyId = 0;
        if (!_bodyInterface || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;

        const JPH::RVec3 position = _bodyInterface->GetPosition(id);
        const JPH::Quat rotation = _bodyInterface->GetRotation(id);
        transform.pos = Math::Vector3(static_cast<float>(position.GetX()), static_cast<float>(position.GetY()), static_cast<float>(position.GetZ()));
        transform.rot = Math::Quaternion(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW());
        return true;
    }

    bool PhysicsJoltBackend::HasBody(PhysicsBackendBodyToken token) const
    {
        std::uint32_t backendBodyId = 0;
        if (!_bodyInterface || !ResolveBodyId(token, backendBodyId))
            return false;
        return _bodyInterface->IsAdded(JPH::BodyID(backendBodyId));
    }

    std::size_t PhysicsJoltBackend::GetBodyCount() const noexcept
    {
        std::lock_guard lock(_bodySetMutex);
        return _bodyIds.size();
    }

    bool PhysicsJoltBackend::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit)
    {
        outHit = {};
        if (!_initialized || !_physicsSystem || !IsFinite(origin) || !IsFinite(direction) || !std::isfinite(maxDistance) || maxDistance <= 0.0f)
            return false;

        JPH::Vec3 castDirection(direction.x, direction.y, direction.z);
        if (castDirection.LengthSq() <= 0.0f)
            return false;
        castDirection = castDirection.Normalized() * maxDistance;

        const JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), castDirection);
        JPH::RayCastResult hit;
        const JPH::NarrowPhaseQuery& query = _physicsSystem->GetNarrowPhaseQuery();
        if (query.CastRay(ray, hit))
        {
            JPH::BodyLockRead lock(_physicsSystem->GetBodyLockInterface(), hit.mBodyID);
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();
                const PhysicsBodyHandle handle = PhysicsBodyHandle::FromValue(body.GetUserData());
                if (!handle || !IsTrackedBodyId(hit.mBodyID.GetIndexAndSequenceNumber()))
                    return false;

                outHit.hasHit = true;
                outHit.bodyHandle = handle;
                outHit.distance = hit.mFraction * maxDistance;
                const JPH::RVec3 hitPos = ray.GetPointOnRay(hit.mFraction);
                outHit.point = Math::Vector3(static_cast<float>(hitPos.GetX()), static_cast<float>(hitPos.GetY()), static_cast<float>(hitPos.GetZ()));
                const JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPos);
                outHit.normal = Math::Vector3(normal.GetX(), normal.GetY(), normal.GetZ());
                return true;
            }
        }

        return false;
    }
} // namespace ChikaEngine::Physics
