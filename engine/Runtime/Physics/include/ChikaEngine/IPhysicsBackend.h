#pragma once

#include "ChikaEngine/PhysicsDescs.h"

#include <vector>

namespace ChikaEngine::Physics
{
    class IPhysicsBackend
    {
      public:
        virtual ~IPhysicsBackend() = default;

        [[nodiscard]] virtual PhysicsResult Initialize(const PhysicsInitDesc& desc) = 0;
        virtual void Shutdown() noexcept = 0;
        [[nodiscard]] virtual bool IsInitialized() const noexcept = 0;
        [[nodiscard]] virtual PhysicsBackendCapabilities GetCapabilities() const noexcept = 0;

        [[nodiscard]] virtual bool Simulate(float dt) = 0;
        [[nodiscard]] virtual PhysicsBodyCreateResult CreateBodyFromDesc(const PhysicsBodyCreateDesc& desc) = 0;
        [[nodiscard]] virtual bool DestroyPhysicsBody(PhysicsBodyHandle handle) = 0;
        [[nodiscard]] virtual bool TrySyncTransform(PhysicsBodyHandle handle, PhysicsTransform& transform) = 0;

        [[nodiscard]] virtual bool SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3& velocity) = 0;
        [[nodiscard]] virtual bool ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3& impulse) = 0;

        [[nodiscard]] virtual bool SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask) = 0;
        [[nodiscard]] virtual PhysicsLayerMask GetLayerCollisionMask(PhysicsLayerID layerId) const = 0;

        [[nodiscard]] virtual bool HasBody(PhysicsBodyHandle handle) const = 0;
        [[nodiscard]] virtual bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit) = 0;
        [[nodiscard]] virtual std::vector<CollisionEvent> PollCollisionEvents() = 0;
        [[nodiscard]] virtual bool SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& pos, const Math::Quaternion& rot) = 0;
    };
} // namespace ChikaEngine::Physics
