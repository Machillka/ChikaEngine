#pragma once

#include "PhysicsDescs.h"

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
        virtual PhysicsBodyHandle CreateBodyFromDesc(const PhysicsBodyCreateDesc& desc) = 0;
        virtual void DestroyPhysicsBody(PhysicsBodyHandle handle) = 0;
        virtual bool TrySyncTransform(PhysicsBodyHandle handle, PhysicsTransform& trans) = 0;

        virtual void SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3 v) = 0;
        virtual void ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3 impulse) = 0;

        virtual void SetLayerMask(std::uint32_t layerIndex, Framework::LayerMask mask) = 0;
        virtual Framework::LayerMask GetLayerMask(std::uint32_t layerIndex) const = 0;

        virtual bool HasRigidbody(PhysicsBodyHandle handle) = 0;

        virtual bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit) = 0;

        virtual std::vector<CollisionEvent> PollCollisionEvents() = 0;
    };
} // namespace ChikaEngine::Physics