#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/subsystem/ISubsystem.h"
#include <memory>
namespace ChikaEngine::Framework
{
    class Scene;
    class PhysicsSubsystem : ISubsystem
    {
      public:
        PhysicsSubsystem(Scene* scene);
        // void Initialize(Scene* scene) override;
        Physics::PhysicsScene* GetPhysicsInstace();
        void Tick(float dt) override;
        void Cleanup() override;
        void ResetSceneState();

      public:
        bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, Physics::RaycastHit& outHit);
        Physics::PhysicsResult QueueCreateBody(const Physics::PhysicsBodyCreateDesc& desc);
        Physics::PhysicsResult QueueRebuildBody(const Physics::PhysicsBodyCreateDesc& desc);
        Physics::PhysicsResult QueueDestroyBody(Core::GameObjectID ownerId);

        bool SetLinearVelocity(Physics::PhysicsBodyHandle handle, const Math::Vector3& velocity);
        bool ApplyForce(Physics::PhysicsBodyHandle handle, const Math::Vector3& force, Physics::PhysicsWakePolicy wakePolicy = Physics::PhysicsWakePolicy::Wake);
        bool ApplyImpulse(Physics::PhysicsBodyHandle handle, const Math::Vector3& impulse);

        void SyncTransform();
        /** @brief Projects and dispatches post-step contact events on the Scene main thread. */
        void DispatchEvents();

      private:
        Scene* _ownerScene = nullptr;
        std::unique_ptr<Physics::PhysicsScene> _physics = nullptr;
    };
} // namespace ChikaEngine::Framework
