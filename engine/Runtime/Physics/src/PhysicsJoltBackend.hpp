#pragma once

#include "ChikaEngine/IPhysicsBackend.h"
#include "ChikaEngine/PhysicsRuntime.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace JPH
{
    class PhysicsSystem;
    class BodyInterface;
    class TempAllocatorImpl;
    class JobSystemThreadPool;
    class Shape;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
    template <typename T> class Ref;
} // namespace JPH

namespace ChikaEngine::Physics
{
    class PhysicsJoltBackend final : public IPhysicsBackend
    {
      public:
        PhysicsJoltBackend();
        ~PhysicsJoltBackend() override;

        [[nodiscard]] PhysicsResult Initialize(const PhysicsInitDesc& desc) override;
        void Shutdown() noexcept override;
        [[nodiscard]] bool IsInitialized() const noexcept override;
        [[nodiscard]] PhysicsBackendCapabilities GetCapabilities() const noexcept override;

        [[nodiscard]] bool Simulate(float fixedDeltaTime) override;
        [[nodiscard]] PhysicsBodyCreateResult CreateBodyFromDesc(const PhysicsBodyCreateDesc& desc) override;
        [[nodiscard]] bool DestroyPhysicsBody(PhysicsBodyHandle handle) override;
        [[nodiscard]] bool TrySyncTransform(PhysicsBodyHandle handle, PhysicsTransform& transform) override;
        [[nodiscard]] std::vector<CollisionEvent> PollCollisionEvents() override;
        [[nodiscard]] bool SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3& velocity) override;
        [[nodiscard]] bool ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3& impulse) override;
        [[nodiscard]] bool SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask) override;
        [[nodiscard]] PhysicsLayerMask GetLayerCollisionMask(PhysicsLayerID layerId) const override;
        [[nodiscard]] bool HasBody(PhysicsBodyHandle handle) const override;
        [[nodiscard]] bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit) override;
        [[nodiscard]] bool SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& pos, const Math::Quaternion& rot) override;

      private:
        struct BodySlot
        {
            std::uint32_t backendBodyId = 0xFFFFFFFFu;
            std::uint32_t generation = 1;
            bool occupied = false;
        };

        [[nodiscard]] JPH::Ref<JPH::Shape> CreateShape(const ColliderShapeDesc& desc);
        [[nodiscard]] PhysicsBodyHandle ReserveBodyHandle();
        [[nodiscard]] bool BindBodyHandle(PhysicsBodyHandle handle, std::uint32_t backendBodyId);
        [[nodiscard]] bool ResolveBodyId(PhysicsBodyHandle handle, std::uint32_t& backendBodyId) const;
        [[nodiscard]] bool ReleaseBodyHandle(PhysicsBodyHandle handle);
        void DestroyAllBodies() noexcept;

        std::unique_ptr<JPH::TempAllocatorImpl> _tempAllocator;
        std::unique_ptr<JPH::JobSystemThreadPool> _jobSystem;
        std::unique_ptr<JPH::PhysicsSystem> _physicsSystem;
        JPH::BodyInterface* _bodyInterface = nullptr;

        PhysicsRuntime::Lease _runtimeLease;
        bool _initialized = false;

        mutable std::mutex _bodyRegistryMutex;
        std::vector<BodySlot> _bodySlots;
        std::vector<std::uint32_t> _freeBodySlots;

        std::mutex _eventMutex;
        std::vector<CollisionEvent> _eventQueue;
        class JoltBackendContactListener;
        std::unique_ptr<JoltBackendContactListener> _listener;
        void PushEvent(const CollisionEvent& event);

        std::unique_ptr<JPH::BroadPhaseLayerInterface> _bpInterface;
        std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> _objVsBPFilter;
        std::unique_ptr<JPH::ObjectLayerPairFilter> _pairFilter;

        mutable std::mutex _maskMutex;
        std::vector<PhysicsLayerMask> _masks;

        mutable std::mutex _commandMutex;
        std::vector<VelocityCommand> _velocityCommands;
        std::vector<ImpulseCommand> _impulseCommands;

        PhysicsJoltBackend(const PhysicsJoltBackend&) = delete;
        PhysicsJoltBackend& operator=(const PhysicsJoltBackend&) = delete;
    };
} // namespace ChikaEngine::Physics
