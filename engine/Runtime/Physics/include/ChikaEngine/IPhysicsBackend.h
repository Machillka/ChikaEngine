#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsEvents.hpp"

#include <vector>

namespace ChikaEngine::Physics
{
    struct PhysicsBackendBodyCreateResult
    {
        PhysicsResult result = PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics backend is not initialized");
        PhysicsBackendBodyToken token = PhysicsBackendBodyToken::Invalid();

        [[nodiscard]] bool Succeeded() const noexcept
        {
            return result.Succeeded() && token.IsValid();
        }

        explicit operator bool() const noexcept
        {
            return Succeeded();
        }
    };

    class IPhysicsBackend
    {
      public:
        virtual ~IPhysicsBackend() = default;

        [[nodiscard]] virtual PhysicsResult Initialize(const PhysicsInitDesc& desc) = 0;
        virtual void Shutdown() noexcept = 0;
        [[nodiscard]] virtual bool IsInitialized() const noexcept = 0;
        [[nodiscard]] virtual PhysicsBackendCapabilities GetCapabilities() const noexcept = 0;
        virtual void ClearBodies() noexcept = 0;

        [[nodiscard]] virtual bool Simulate(float dt, std::uint64_t fixedStepIndex) = 0;
        [[nodiscard]] virtual PhysicsBackendBodyCreateResult CreateBodyFromDesc(PhysicsBodyHandle engineHandle, const PhysicsBodyCreateDesc& desc) = 0;
        [[nodiscard]] virtual bool DestroyPhysicsBody(PhysicsBackendBodyToken token) = 0;
        [[nodiscard]] virtual bool TrySyncTransform(PhysicsBackendBodyToken token, PhysicsTransform& transform) = 0;
        /** @brief Returns value snapshots for active Dynamic bodies only. Called after Simulate on the owner thread. */
        [[nodiscard]] virtual std::vector<PhysicsBodySnapshot> CollectActiveDynamicBodySnapshots() = 0;

        [[nodiscard]] virtual bool SetLinearVelocity(PhysicsBackendBodyToken token, const Math::Vector3& velocity) = 0;
        [[nodiscard]] virtual bool SetAngularVelocity(PhysicsBackendBodyToken token, const Math::Vector3& velocity) = 0;
        [[nodiscard]] virtual bool AddForce(PhysicsBackendBodyToken token, const Math::Vector3& force, PhysicsWakePolicy wakePolicy) = 0;
        [[nodiscard]] virtual bool AddTorque(PhysicsBackendBodyToken token, const Math::Vector3& torque, PhysicsWakePolicy wakePolicy) = 0;
        [[nodiscard]] virtual bool ApplyImpulse(PhysicsBackendBodyToken token, const Math::Vector3& impulse) = 0;
        [[nodiscard]] virtual bool ApplyAngularImpulse(PhysicsBackendBodyToken token, const Math::Vector3& impulse) = 0;
        [[nodiscard]] virtual bool SetBodyActive(PhysicsBackendBodyToken token, bool active) = 0;
        [[nodiscard]] virtual bool TeleportBody(PhysicsBackendBodyToken token, const Math::Vector3& position, const Math::Quaternion& rotation, bool resetVelocity, PhysicsWakePolicy wakePolicy) = 0;
        [[nodiscard]] virtual bool SetKinematicTarget(PhysicsBackendBodyToken token, const Math::Vector3& position, const Math::Quaternion& rotation, float deltaTime) = 0;

        [[nodiscard]] virtual bool SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask) = 0;
        [[nodiscard]] virtual PhysicsLayerMask GetLayerCollisionMask(PhysicsLayerID layerId) const = 0;

        [[nodiscard]] virtual bool HasBody(PhysicsBackendBodyToken token) const = 0;
        [[nodiscard]] virtual std::size_t GetBodyCount() const noexcept = 0;
        [[nodiscard]] virtual bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit) = 0;
        /** @brief Drains callback packets after Simulate has returned; never publishes gameplay events. */
        [[nodiscard]] virtual std::vector<RawContactPacket> DrainRawContactPackets() = 0;
    };
} // namespace ChikaEngine::Physics
