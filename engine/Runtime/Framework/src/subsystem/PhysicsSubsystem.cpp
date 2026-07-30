#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/PhysicsCallbackEvents.hpp"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"
#include "ChikaEngine/scene/scene.hpp"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace ChikaEngine::Framework
{
    namespace
    {
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

        // FIXME: 非常的不现代, 效率极低
        auto physicsTransforms = _physics->PollTransform();
        // LOG_INFO("Physics Subsystem", "size of padding: {}, id = {}", physicsTransforms.size(), physicsTransforms[0].first);

        for (auto const& [goId, physicsTransform] : physicsTransforms)
        {
            auto go = _ownerScene->GetGameObject(goId);

            if (!go)
                continue;

            // LOG_INFO("Sync Transform", "ID = {}, originY: {}, phyY: {}", goId, go->transform->position.y, physicsTransform.pos.y);
            go->transform->SetWorldPositionAndRotation(physicsTransform.pos, physicsTransform.rot);
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
