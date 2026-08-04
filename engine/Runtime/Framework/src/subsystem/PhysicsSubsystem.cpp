#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/PhysicsCallbackEvents.hpp"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/component/Collider.hpp"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"
#include "ChikaEngine/scene/scene.hpp"

#include <memory>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>
#include <vector>

namespace ChikaEngine::Framework
{
    namespace
    {
        constexpr float TransformEpsilon = 1.0e-5f;

        bool NearlyEqual(float lhs, float rhs) noexcept
        {
            return std::abs(lhs - rhs) <= TransformEpsilon;
        }

        bool NearlyEqual(const Math::Vector3& lhs, const Math::Vector3& rhs) noexcept
        {
            return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z);
        }

        bool NearlyEqualRotation(const Math::Quaternion& lhs, const Math::Quaternion& rhs) noexcept
        {
            return std::abs(std::abs(lhs.Normalized().Dot(rhs.Normalized())) - 1.0f) <= TransformEpsilon;
        }

        void Negate(Math::Vector3& value) noexcept
        {
            value.x = -value.x;
            value.y = -value.y;
            value.z = -value.z;
        }

        PhysicsContactEvent ProjectOwnerView(const Physics::PhysicsPairEvent& event, bool selfIsA, bool selfAlive, bool otherAlive)
        {
            PhysicsContactEvent view{
                .phase = event.phase,
                .kind = event.kind,
                .selfBody = selfIsA ? event.pair.bodyA : event.pair.bodyB,
                .otherBody = selfIsA ? event.pair.bodyB : event.pair.bodyA,
                .selfCollider = selfIsA ? event.pair.colliderA : event.pair.colliderB,
                .otherCollider = selfIsA ? event.pair.colliderB : event.pair.colliderA,
                .selfGameObject = selfIsA ? event.gameObjectA : event.gameObjectB,
                .otherGameObject = selfIsA ? event.gameObjectB : event.gameObjectA,
                .contact = event.contact,
                .terminationReason = event.terminationReason,
                .hasContactData = event.hasContactData,
                .selfAlive = selfAlive,
                .otherAlive = otherAlive,
                .fixedStepIndex = event.fixedStepIndex,
            };
            if (!selfIsA)
            {
                if (view.contact.hasNormal)
                    Negate(view.contact.normal);
                if (view.contact.hasRelativeVelocity)
                    Negate(view.contact.relativeVelocity);
            }
            return view;
        }
    } // namespace

    PhysicsSubsystem::PhysicsSubsystem(Scene* scene) : _ownerScene(scene)
    {

        Physics::PhysicsSystemDesc createInfo{};

        _physics = std::make_unique<Physics::PhysicsScene>(createInfo);
        if (!_physics->IsInitialized())
        {
            const Physics::PhysicsResult& result = _physics->GetInitializationResult();
            LOG_ERROR("Physics Subsystem", "Failed to initialize physics scene: {}", result.diagnostic);
        }
    }

    Physics::PhysicsScene* PhysicsSubsystem::GetPhysicsInstace()
    {
        return _physics.get();
    }

    void PhysicsSubsystem::Tick(float dt)
    {
        CHIKA_PROFILE_SCOPE("Physics.Simulate");
        if (_physics)
            _physics->Tick(dt);
    }

    void PhysicsSubsystem::Cleanup()
    {
        _authorityStates.clear();
        _interpolationStates.clear();
        if (_physics)
        {
            _physics->Shutdown();
            _physics.reset();
        }
    }

    void PhysicsSubsystem::ResetSceneState()
    {
        if (_physics)
            _physics->ResetSceneState();
        _authorityStates.clear();
        _interpolationStates.clear();
        _renderInterpolationAlpha = 1.0f;
    }

    void PhysicsSubsystem::PrepareTransforms(float)
    {
        if (!_physics || !_ownerScene)
            return;

        for (const auto& ownedObject : _ownerScene->GetAllGameobjects())
        {
            GameObject* object = ownedObject.get();
            if (!object || object->IsPendingDestroy() || !object->transform)
                continue;
            Collider* collider = object->GetComponent<Collider>();
            if (!collider || !collider->IsActiveAndEnabled())
                continue;

            Rigidbody* rigidbody = object->GetComponent<Rigidbody>();
            const Physics::MotionType motionType = rigidbody && rigidbody->IsActiveAndEnabled() ? rigidbody->GetMotionType() : Physics::MotionType::Static;
            if (motionType != Physics::MotionType::Dynamic)
                _interpolationStates.erase(object->GetID());
            const Math::Vector3 worldPosition = object->transform->GetWorldPosition();
            const Math::Quaternion worldRotation = object->transform->GetWorldRotation();
            const Math::Vector3 worldScale = object->transform->GetWorldScale();
            const Physics::PhysicsBodyHandle handle = _physics->GetBodyHandle(object->GetID());
            auto& state = _authorityStates[object->GetID()];

            if (!state.initialized || state.motionType != motionType)
            {
                state = {
                    .handle = handle,
                    .motionType = motionType,
                    .transform = { .pos = worldPosition, .rot = worldRotation },
                    .worldScale = worldScale,
                    .initialized = true,
                };
                if (motionType == Physics::MotionType::Dynamic)
                {
                    if (const auto snapshot = _physics->GetBodySnapshot(handle))
                    {
                        state.transform = snapshot->transform;
                        object->transform->SetWorldPositionAndRotation(snapshot->transform.pos, snapshot->transform.rot);
                    }
                }
                continue;
            }

            if (state.handle != handle)
            {
                state.handle = handle;
                if (motionType == Physics::MotionType::Dynamic)
                {
                    if (const auto snapshot = _physics->GetBodySnapshot(handle))
                    {
                        state.transform = snapshot->transform;
                        state.worldScale = worldScale;
                        object->transform->SetWorldPositionAndRotation(snapshot->transform.pos, snapshot->transform.rot);
                    }
                    continue;
                }
            }

            const bool poseChanged = !NearlyEqual(worldPosition, state.transform.pos) || !NearlyEqualRotation(worldRotation, state.transform.rot);
            const bool scaleChanged = !NearlyEqual(worldScale, state.worldScale);
            if (motionType == Physics::MotionType::Static)
            {
                if (poseChanged || scaleChanged)
                    collider->RequestBodyRebuild();
                state.transform = { .pos = worldPosition, .rot = worldRotation };
                state.worldScale = worldScale;
            }
            else if (motionType == Physics::MotionType::Kinematic)
            {
                if (scaleChanged)
                    collider->RequestBodyRebuild();
                if (poseChanged && handle)
                    (void)_physics->QueueKinematicTarget(Physics::PhysicsBodyTarget::FromHandle(handle), worldPosition, worldRotation);
                state.transform = { .pos = worldPosition, .rot = worldRotation };
                state.worldScale = worldScale;
            }
            else
            {
                // Dynamic pose is physics-owned. Direct Transform writes are
                // rejected by restoring the latest main-thread snapshot.
                if (poseChanged)
                    object->transform->SetWorldPositionAndRotation(state.transform.pos, state.transform.rot);
                if (scaleChanged)
                {
                    collider->RequestBodyRebuild();
                    state.worldScale = worldScale;
                }
            }
        }
    }

    void PhysicsSubsystem::SetRenderInterpolationAlpha(float alpha)
    {
        _renderInterpolationAlpha = std::clamp(alpha, 0.0f, 1.0f);
    }

    Math::Mat4 PhysicsSubsystem::GetRenderWorldMatrix(const Transform& transform) const
    {
        const GameObject* owner = transform.GetOwner();
        if (owner)
        {
            const auto interpolation = _interpolationStates.find(owner->GetID());
            if (interpolation != _interpolationStates.end())
            {
                const Physics::PhysicsTransform& previous = interpolation->second.previous;
                const Physics::PhysicsTransform& current = interpolation->second.current;
                return Math::Mat4::TRSMatrix(Math::Vector3::Lerp(previous.pos, current.pos, _renderInterpolationAlpha), Math::Quaternion::Slerp(previous.rot, current.rot, _renderInterpolationAlpha), transform.GetWorldScale());
            }
        }
        return transform.GetParent() ? GetRenderWorldMatrix(*transform.GetParent()) * transform.GetLocalMatrix() : transform.GetLocalMatrix();
    }

    bool PhysicsSubsystem::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, Physics::RaycastHit& outHit)
    {
        return _physics && _physics->Raycast(origin, direction, maxDistance, outHit);
    }

    Physics::PhysicsResult PhysicsSubsystem::QueueCreateBody(const Physics::PhysicsBodyCreateDesc& desc)
    {
        if (!_physics)
            return Physics::PhysicsResult::Failure(Physics::PhysicsStatus::NotInitialized, "Physics subsystem is not initialized");
        return _physics->QueueCreateBody(desc);
    }

    Physics::PhysicsResult PhysicsSubsystem::QueueRebuildBody(const Physics::PhysicsBodyCreateDesc& desc)
    {
        if (!_physics)
            return Physics::PhysicsResult::Failure(Physics::PhysicsStatus::NotInitialized, "Physics subsystem is not initialized");
        return _physics->QueueRebuildBody(desc);
    }

    Physics::PhysicsResult PhysicsSubsystem::QueueDestroyBody(Core::GameObjectID ownerId)
    {
        if (!_physics)
            return Physics::PhysicsResult::Failure(Physics::PhysicsStatus::NotInitialized, "Physics subsystem is not initialized");
        return _physics->QueueDestroyBody(ownerId);
    }

    bool PhysicsSubsystem::SetLinearVelocity(Physics::PhysicsBodyHandle handle, const Math::Vector3& velocity)
    {
        return _physics && _physics->SetLinearVelocity(handle, velocity);
    }

    bool PhysicsSubsystem::ApplyImpulse(Physics::PhysicsBodyHandle handle, const Math::Vector3& impulse)
    {
        return _physics && _physics->ApplyImpulse(handle, impulse);
    }

    bool PhysicsSubsystem::ApplyForce(Physics::PhysicsBodyHandle handle, const Math::Vector3& force, Physics::PhysicsWakePolicy wakePolicy)
    {
        return _physics && _physics->ApplyForce(handle, force, wakePolicy);
    }

    void PhysicsSubsystem::SyncTransform()
    {
        CHIKA_PROFILE_SCOPE("Physics.SyncTransforms");
        if (!_physics)
            return;

        const auto& physicsTransforms = _physics->PollActiveDynamicSnapshots();

        for (auto const& [goId, snapshot] : physicsTransforms)
        {
            auto go = _ownerScene->GetGameObject(goId);

            if (!go)
                continue;

            auto& interpolation = _interpolationStates[goId];
            if (interpolation.handle != snapshot.handle)
            {
                interpolation = {
                    .handle = snapshot.handle,
                    .previous = snapshot.transform,
                    .current = snapshot.transform,
                };
            }
            else
            {
                interpolation.previous = interpolation.current;
                interpolation.current = snapshot.transform;
            }
            go->transform->SetWorldPositionAndRotation(snapshot.transform.pos, snapshot.transform.rot);
            auto& authority = _authorityStates[goId];
            authority.handle = snapshot.handle;
            authority.motionType = Physics::MotionType::Dynamic;
            authority.transform = snapshot.transform;
            authority.worldScale = go->transform->GetWorldScale();
            authority.initialized = true;
        }
    }

    void PhysicsSubsystem::DispatchEvents()
    {
        CHIKA_PROFILE_SCOPE("Physics.DispatchEvents");
        if (!_physics || !_ownerScene)
            return;

        const std::vector<Physics::PhysicsPairEvent> events = _physics->DrainPairEvents();
        for (const Physics::PhysicsPairEvent& event : events)
        {
            auto resolveParticipant = [this](Core::GameObjectID gameObjectId, Physics::PhysicsBodyHandle bodyHandle)
            {
                GameObject* gameObject = _ownerScene->GetGameObject(gameObjectId);
                const bool alive = gameObject && !gameObject->IsPendingDestroy() && _physics->HasBody(bodyHandle);
                return std::pair<GameObject*, bool>{ gameObject, alive };
            };

            auto [gameObjectA, aliveA] = resolveParticipant(event.gameObjectA, event.pair.bodyA);
            auto [gameObjectB, aliveB] = resolveParticipant(event.gameObjectB, event.pair.bodyB);
            const PhysicsContactEvent viewA = ProjectOwnerView(event, true, aliveA, aliveB);
            if (aliveA)
                gameObjectA->DispatchPhysicsEvent(viewA);

            // A-side callbacks may destroy/disable B. Re-resolve before B-side
            // dispatch so a pending-destroy participant never receives a callback.
            std::tie(gameObjectA, aliveA) = resolveParticipant(event.gameObjectA, event.pair.bodyA);
            std::tie(gameObjectB, aliveB) = resolveParticipant(event.gameObjectB, event.pair.bodyB);
            const PhysicsContactEvent viewB = ProjectOwnerView(event, false, aliveB, aliveA);
            if (aliveB)
                gameObjectB->DispatchPhysicsEvent(viewB);

            _ownerScene->GetEventBus().Publish(event);
        }
    }
} // namespace ChikaEngine::Framework
