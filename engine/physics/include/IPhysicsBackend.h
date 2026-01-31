#pragma once

#include "PhysicsDescs.h"
#include "component/Transform.h"

#include <vector>
namespace ChikaEngine::Physics
{

    class IPhysicsBackend
    {
      public:
        virtual ~IPhysicsBackend() = default;
        virtual bool Initialize(const PhysicsInitDesc& desc) = 0;
        virtual void Shutdown() = 0;
        virtual void Simulate(float dt) = 0;
        virtual PhysicsBodyHandle CreateBodyFromDesc(const RigidbodyCreateDesc& desc) = 0;
        virtual void DestroyPhysicsBody(PhysicsBodyHandle handle) = 0;
        virtual bool TrySyncTransform(PhysicsBodyHandle handle, Framework::Transform& trans) = 0;
        virtual std::vector<CollisionEvent> PollCollisionEvents() = 0;
    };
} // namespace ChikaEngine::Physics