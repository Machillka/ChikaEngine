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
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/AllowedDOFs.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
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
#include <unordered_map>
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

        class QueryEnabledBodyFilter final : public JPH::BodyFilter
        {
          public:
            QueryEnabledBodyFilter(const std::unordered_set<std::uint32_t>& disabledBodyIds, std::mutex& mutex) : _disabledBodyIds(disabledBodyIds), _mutex(mutex) {}

            bool ShouldCollide(const JPH::BodyID& bodyId) const override
            {
                std::lock_guard lock(_mutex);
                return !_disabledBodyIds.contains(bodyId.GetIndexAndSequenceNumber());
            }

          private:
            const std::unordered_set<std::uint32_t>& _disabledBodyIds;
            std::mutex& _mutex;
        };
    } // namespace

    class PhysicsJoltBackend::JoltBackendContactListener final : public JPH::ContactListener
    {
      public:
        explicit JoltBackendContactListener(PhysicsJoltBackend* physicsBackend) : _physicsBackend(physicsBackend) {}

        void OnContactAdded(const JPH::Body& bodyA, const JPH::Body& bodyB, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override
        {
            CaptureContact(RawContactPhase::Added, bodyA, bodyB, manifold, settings);
        }

        void OnContactPersisted(const JPH::Body& bodyA, const JPH::Body& bodyB, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override
        {
            CaptureContact(RawContactPhase::Persisted, bodyA, bodyB, manifold, settings);
        }

        void OnContactRemoved(const JPH::SubShapeIDPair& subShapePair) override
        {
            if (!_physicsBackend)
                return;

            CachedContactIdentity identity;
            {
                std::lock_guard lock(_contactMutex);
                const auto it = _contactIdentities.find(subShapePair);
                if (it == _contactIdentities.end())
                    return;
                identity = it->second;
                _contactIdentities.erase(it);
            }

            RawContactPacket packet;
            packet.phase = RawContactPhase::Removed;
            packet.bodyA = identity.bodyA;
            packet.bodyB = identity.bodyB;
            packet.feature = identity.feature;
            packet.isSensorPair = identity.isSensorPair;
            _physicsBackend->PushRawContactPacket(packet);
        }

        void ClearCachedContacts()
        {
            std::lock_guard lock(_contactMutex);
            _contactIdentities.clear();
        }

      private:
        struct CachedContactIdentity
        {
            PhysicsBodyHandle bodyA = PhysicsBodyHandle::Invalid();
            PhysicsBodyHandle bodyB = PhysicsBodyHandle::Invalid();
            PhysicsContactFeatureKey feature;
            bool isSensorPair = false;
        };

        void CaptureContact(RawContactPhase phase, const JPH::Body& bodyA, const JPH::Body& bodyB, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings)
        {
            if (!_physicsBackend)
                return;

            const PhysicsBodyHandle handleA = PhysicsBodyHandle::FromValue(bodyA.GetUserData());
            const PhysicsBodyHandle handleB = PhysicsBodyHandle::FromValue(bodyB.GetUserData());
            if (!handleA || !handleB)
                return;

            RawContactPacket packet;
            packet.phase = phase;
            packet.bodyA = handleA;
            packet.bodyB = handleB;
            packet.feature = {
                .featureA = manifold.mSubShapeID1.GetValue(),
                .featureB = manifold.mSubShapeID2.GetValue(),
            };
            packet.isSensorPair = settings.mIsSensor || bodyA.IsSensor() || bodyB.IsSensor();
            packet.bodyAExists = true;
            packet.bodyBExists = true;
            packet.bodyAActive = bodyA.IsActive();
            packet.bodyBActive = bodyB.IsActive();

            const JPH::Vec3 normal = manifold.mWorldSpaceNormal;
            packet.contact.normal = Math::Vector3(normal.GetX(), normal.GetY(), normal.GetZ());
            packet.contact.hasNormal = true;
            packet.contact.penetration = manifold.mPenetrationDepth;
            packet.contact.hasPenetration = true;

            if (!manifold.mRelativeContactPointsOn1.empty())
            {
                const JPH::RVec3 point = manifold.GetWorldSpaceContactPointOn1(0);
                packet.contact.point = Math::Vector3(static_cast<float>(point.GetX()), static_cast<float>(point.GetY()), static_cast<float>(point.GetZ()));
                packet.contact.hasPoint = true;
                const JPH::Vec3 relativeVelocity = bodyB.GetPointVelocity(point) - bodyA.GetPointVelocity(point);
                packet.contact.relativeVelocity = Math::Vector3(relativeVelocity.GetX(), relativeVelocity.GetY(), relativeVelocity.GetZ());
                packet.contact.hasRelativeVelocity = true;
            }

            const JPH::SubShapeIDPair subShapePair(bodyA.GetID(), manifold.mSubShapeID1, bodyB.GetID(), manifold.mSubShapeID2);
            {
                std::lock_guard lock(_contactMutex);
                _contactIdentities[subShapePair] = CachedContactIdentity{
                    .bodyA = handleA,
                    .bodyB = handleB,
                    .feature = packet.feature,
                    .isSensorPair = packet.isSensorPair,
                };
            }
            _physicsBackend->PushRawContactPacket(packet);
        }

        PhysicsJoltBackend* _physicsBackend = nullptr;
        std::mutex _contactMutex;
        std::unordered_map<JPH::SubShapeIDPair, CachedContactIdentity> _contactIdentities;
    };

    PhysicsJoltBackend::PhysicsJoltBackend()
    {
        _masks.resize(PHYSICS_LAYER_COUNT, PHYSICS_LAYER_MASK_ALL);
    }

    PhysicsJoltBackend::~PhysicsJoltBackend()
    {
        Shutdown();
    }

    void PhysicsJoltBackend::PushRawContactPacket(RawContactPacket packet)
    {
        packet.fixedStepIndex = _currentFixedStepIndex.load(std::memory_order_relaxed);
        packet.sequence = _nextContactSequence.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(_eventMutex);
        _eventQueue.push_back(packet);
    }

    std::vector<RawContactPacket> PhysicsJoltBackend::DrainRawContactPackets()
    {
        std::vector<RawContactPacket> snapshot;
        {
            std::lock_guard lock(_eventMutex);
            snapshot.swap(_eventQueue);
        }
        for (RawContactPacket& packet : snapshot)
        {
            if (packet.phase == RawContactPhase::Removed)
                EnrichRemovalState(packet);
        }
        return snapshot;
    }

    void PhysicsJoltBackend::EnrichRemovalState(RawContactPacket& packet) const
    {
        std::uint32_t bodyIdA = JPH::BodyID::cInvalidBodyID;
        std::uint32_t bodyIdB = JPH::BodyID::cInvalidBodyID;
        {
            std::lock_guard lock(_bodySetMutex);
            const auto itA = _bodyIdByEngineHandle.find(packet.bodyA);
            const auto itB = _bodyIdByEngineHandle.find(packet.bodyB);
            if (itA != _bodyIdByEngineHandle.end())
                bodyIdA = itA->second;
            if (itB != _bodyIdByEngineHandle.end())
                bodyIdB = itB->second;
        }

        const JPH::BodyID joltBodyA(bodyIdA);
        const JPH::BodyID joltBodyB(bodyIdB);
        packet.bodyAExists = _bodyInterface && bodyIdA != JPH::BodyID::cInvalidBodyID && _bodyInterface->IsAdded(joltBodyA);
        packet.bodyBExists = _bodyInterface && bodyIdB != JPH::BodyID::cInvalidBodyID && _bodyInterface->IsAdded(joltBodyB);
        packet.bodyAActive = packet.bodyAExists && _bodyInterface->IsActive(joltBodyA);
        packet.bodyBActive = packet.bodyBExists && _bodyInterface->IsActive(joltBodyB);

        if (!packet.bodyAExists || !packet.bodyBExists)
        {
            packet.removalState = RawContactRemovalState::BodyMissing;
            return;
        }
        if (_physicsSystem && _physicsSystem->WereBodiesInContact(joltBodyA, joltBodyB))
        {
            packet.removalState = RawContactRemovalState::OtherContactActive;
            return;
        }
        if (!packet.bodyAActive && !packet.bodyBActive)
        {
            packet.removalState = RawContactRemovalState::Deactivated;
            return;
        }
        packet.removalState = RawContactRemovalState::Separated;
    }

    JPH::Ref<JPH::Shape> PhysicsJoltBackend::CreateShape(const ColliderShapeDesc& desc)
    {
        auto applyCenter = [&desc](JPH::Ref<JPH::Shape> shape) -> JPH::Ref<JPH::Shape>
        {
            if (!shape || (desc.center.x == 0.0f && desc.center.y == 0.0f && desc.center.z == 0.0f))
                return shape;

            JPH::RotatedTranslatedShapeSettings settings(JPH::Vec3(desc.center.x, desc.center.y, desc.center.z), JPH::Quat::sIdentity(), shape.GetPtr());
            auto result = settings.Create();
            if (result.HasError())
            {
                LOG_ERROR("Physics", "Unable to apply Collider center offset: {}", result.GetError());
                return nullptr;
            }
            return result.Get();
        };

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
            return applyCenter(shape);
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
            return applyCenter(shape);
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
        _currentFixedStepIndex.store(0, std::memory_order_relaxed);
        _nextContactSequence.store(1, std::memory_order_relaxed);
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
            .continuousCollisionDetection = true,
        };
    }

    void PhysicsJoltBackend::ClearBodies() noexcept
    {
        DestroyAllBodies();
        if (_listener)
            _listener->ClearCachedContacts();
        std::lock_guard lock(_eventMutex);
        _eventQueue.clear();
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
            _queryDisabledBodyIds.clear();
            _bodyIdByEngineHandle.clear();
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

    bool PhysicsJoltBackend::SetAngularVelocity(PhysicsBackendBodyToken token, const Math::Vector3& velocity)
    {
        std::uint32_t backendBodyId = 0;
        if (!_bodyInterface || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;
        _bodyInterface->SetAngularVelocity(id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
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

    bool PhysicsJoltBackend::AddTorque(PhysicsBackendBodyToken token, const Math::Vector3& torque, PhysicsWakePolicy wakePolicy)
    {
        std::uint32_t backendBodyId = 0;
        if (!_bodyInterface || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;
        _bodyInterface->AddTorque(id, JPH::Vec3(torque.x, torque.y, torque.z), ToJoltActivation(wakePolicy, _bodyInterface->IsActive(id)));
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

    bool PhysicsJoltBackend::ApplyAngularImpulse(PhysicsBackendBodyToken token, const Math::Vector3& impulse)
    {
        std::uint32_t backendBodyId = 0;
        if (!_bodyInterface || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;
        _bodyInterface->AddAngularImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
        return true;
    }

    bool PhysicsJoltBackend::SetBodyActive(PhysicsBackendBodyToken token, bool active)
    {
        std::uint32_t backendBodyId = 0;
        if (!_bodyInterface || !ResolveBodyId(token, backendBodyId))
            return false;
        const JPH::BodyID id(backendBodyId);
        if (!_bodyInterface->IsAdded(id))
            return false;
        if (active)
            _bodyInterface->ActivateBody(id);
        else
            _bodyInterface->DeactivateBody(id);
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

    bool PhysicsJoltBackend::Simulate(float fixedDeltaTime, std::uint64_t fixedStepIndex)
    {
        if (!_initialized || !_physicsSystem || !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f || fixedStepIndex == 0)
            return false;

        _currentFixedStepIndex.store(fixedStepIndex, std::memory_order_relaxed);
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
        if (!IsFinite(desc.position) || !IsFinite(desc.rotation) || !IsNormalized(desc.rotation) || !IsValidShape(desc.shapeDesc) || !std::isfinite(desc.mass) || !std::isfinite(desc.friction) || !std::isfinite(desc.restitution) || !std::isfinite(desc.linearDamping) || !std::isfinite(desc.angularDamping) || !std::isfinite(desc.gravityFactor) || desc.mass <= 0.0f || desc.friction < 0.0f || desc.restitution < 0.0f || desc.restitution > 1.0f || desc.linearDamping < 0.0f ||
            desc.angularDamping < 0.0f || desc.axisLockMask > PhysicsAxisLockAll || (desc.motionType != MotionType::Static && desc.axisLockMask == PhysicsAxisLockAll))
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
            settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = desc.mass;
            settings.mAllowDynamicOrKinematic = true;
            settings.mLinearDamping = desc.linearDamping;
            settings.mAngularDamping = desc.angularDamping;
            settings.mGravityFactor = desc.gravityFactor;
            settings.mAllowSleeping = desc.allowSleeping;
            settings.mMotionQuality = desc.continuousCollisionDetection ? EMotionQuality::LinearCast : EMotionQuality::Discrete;
            settings.mAllowedDOFs = static_cast<EAllowedDOFs>(static_cast<std::uint8_t>(PhysicsAxisLockAll) & static_cast<std::uint8_t>(~desc.axisLockMask));
        }

        settings.mIsSensor = desc.isTrigger;
        settings.mFriction = desc.friction;
        settings.mRestitution = desc.restitution;
        BodyID id = _bodyInterface->CreateAndAddBody(settings, EActivation::Activate);
        if (id.IsInvalid())
            return { .result = PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Jolt failed to create physics body") };

        {
            std::lock_guard lock(_bodySetMutex);
            const std::uint32_t backendBodyId = id.GetIndexAndSequenceNumber();
            _bodyIds.insert(backendBodyId);
            if (!desc.queryEnabled)
                _queryDisabledBodyIds.insert(backendBodyId);
            _bodyIdByEngineHandle[engineHandle] = backendBodyId;
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
        const bool erased = _bodyIds.erase(backendBodyId) == 1;
        _queryDisabledBodyIds.erase(backendBodyId);
        for (auto it = _bodyIdByEngineHandle.begin(); it != _bodyIdByEngineHandle.end();)
        {
            if (it->second == backendBodyId)
                it = _bodyIdByEngineHandle.erase(it);
            else
                ++it;
        }
        return erased;
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

    std::vector<PhysicsBodySnapshot> PhysicsJoltBackend::CollectActiveDynamicBodySnapshots()
    {
        std::vector<PhysicsBodySnapshot> snapshots;
        if (!_physicsSystem || !_bodyInterface)
            return snapshots;

        JPH::BodyIDVector activeBodies;
        _physicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);
        snapshots.reserve(activeBodies.size());
        for (const JPH::BodyID& id : activeBodies)
        {
            if (!_bodyInterface->IsAdded(id) || _bodyInterface->GetMotionType(id) != JPH::EMotionType::Dynamic)
                continue;

            const PhysicsBodyHandle handle = PhysicsBodyHandle::FromValue(_bodyInterface->GetUserData(id));
            if (!handle)
                continue;
            const JPH::RVec3 position = _bodyInterface->GetPosition(id);
            const JPH::Quat rotation = _bodyInterface->GetRotation(id);
            const JPH::Vec3 linearVelocity = _bodyInterface->GetLinearVelocity(id);
            const JPH::Vec3 angularVelocity = _bodyInterface->GetAngularVelocity(id);
            snapshots.push_back(PhysicsBodySnapshot{
                .handle = handle,
                .transform = {
                    .pos = { static_cast<float>(position.GetX()), static_cast<float>(position.GetY()), static_cast<float>(position.GetZ()) },
                    .rot = { rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW() },
                },
                .linearVelocity = { linearVelocity.GetX(), linearVelocity.GetY(), linearVelocity.GetZ() },
                .angularVelocity = { angularVelocity.GetX(), angularVelocity.GetY(), angularVelocity.GetZ() },
                .sleeping = false,
            });
        }
        return snapshots;
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
        QueryEnabledBodyFilter bodyFilter(_queryDisabledBodyIds, _bodySetMutex);
        if (query.CastRay(ray, hit, {}, {}, bodyFilter))
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
