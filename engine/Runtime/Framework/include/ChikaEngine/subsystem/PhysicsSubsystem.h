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
        void Cleanup() override {};

      public:
        bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, Physics::RaycastHit& outHit);
        Physics::PhysicsBodyHandle CreateBodyImmediate(const Physics::PhysicsBodyCreateDesc& desc);

        void SetLinearVelocity(Physics::PhysicsBodyHandle handle, const Math::Vector3 v);
        void ApplyImpulse(Physics::PhysicsBodyHandle handle, const Math::Vector3 impulse);

        void SyncTransform();

      private:
        std::unique_ptr<Physics::PhysicsScene> _physics = nullptr;
    };
} // namespace ChikaEngine::Framework