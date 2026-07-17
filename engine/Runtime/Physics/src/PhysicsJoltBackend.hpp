#pragma once

#include "ChikaEngine/IPhysicsBackend.h"
#include "ChikaEngine/PhysicsRuntime.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
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
        void ClearBodies() noexcept override;

        [[nodiscard]] bool Simulate(float fixedDeltaTime, std::uint64_t fixedStepIndex) override;
        [[nodiscard]] PhysicsBackendBodyCreateResult CreateBodyFromDesc(PhysicsBodyHandle engineHandle, const PhysicsBodyCreateDesc& desc) override;
        [[nodiscard]] bool DestroyPhysicsBody(PhysicsBackendBodyToken token) override;
        [[nodiscard]] bool TrySyncTransform(PhysicsBackendBodyToken token, PhysicsTransform& transform) override;
        [[nodiscard]] std::vector<RawContactPacket> DrainRawContactPackets() override;
        [[nodiscard]] bool SetLinearVelocity(PhysicsBackendBodyToken token, const Math::Vector3& velocity) override;
        [[nodiscard]] bool AddForce(PhysicsBackendBodyToken token, const Math::Vector3& force, PhysicsWakePolicy wakePolicy) override;
        [[nodiscard]] bool ApplyImpulse(PhysicsBackendBodyToken token, const Math::Vector3& impulse) override;
        [[nodiscard]] bool TeleportBody(PhysicsBackendBodyToken token, const Math::Vector3& position, const Math::Quaternion& rotation, bool resetVelocity, PhysicsWakePolicy wakePolicy) override;
        [[nodiscard]] bool SetKinematicTarget(PhysicsBackendBodyToken token, const Math::Vector3& position, const Math::Quaternion& rotation, float deltaTime) override;
        [[nodiscard]] bool SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask) override;
        [[nodiscard]] PhysicsLayerMask GetLayerCollisionMask(PhysicsLayerID layerId) const override;
        [[nodiscard]] bool HasBody(PhysicsBackendBodyToken token) const override;
        [[nodiscard]] std::size_t GetBodyCount() const noexcept override;
        [[nodiscard]] bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit) override;

      private:
        [[nodiscard]] JPH::Ref<JPH::Shape> CreateShape(const ColliderShapeDesc& desc);
        [[nodiscard]] bool ResolveBodyId(PhysicsBackendBodyToken token, std::uint32_t& backendBodyId) const;
        [[nodiscard]] bool IsTrackedBodyId(std::uint32_t backendBodyId) const;
        void DestroyAllBodies() noexcept;

        std::unique_ptr<JPH::TempAllocatorImpl> _tempAllocator;
        std::unique_ptr<JPH::JobSystemThreadPool> _jobSystem;
        std::unique_ptr<JPH::PhysicsSystem> _physicsSystem;
        JPH::BodyInterface* _bodyInterface = nullptr;

        PhysicsRuntime::Lease _runtimeLease;
        bool _initialized = false;

        mutable std::mutex _bodySetMutex;
        std::unordered_set<std::uint32_t> _bodyIds;
        std::unordered_map<PhysicsBodyHandle, std::uint32_t, PhysicsHandleHash> _bodyIdByEngineHandle;

        std::mutex _eventMutex;
        std::vector<RawContactPacket> _eventQueue;
        std::atomic<std::uint64_t> _currentFixedStepIndex = 0;
        std::atomic<std::uint64_t> _nextContactSequence = 1;
        class JoltBackendContactListener;
        std::unique_ptr<JoltBackendContactListener> _listener;
        void PushRawContactPacket(RawContactPacket packet);
        void EnrichRemovalState(RawContactPacket& packet) const;

        std::unique_ptr<JPH::BroadPhaseLayerInterface> _bpInterface;
        std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> _objVsBPFilter;
        std::unique_ptr<JPH::ObjectLayerPairFilter> _pairFilter;

        mutable std::mutex _maskMutex;
        std::vector<PhysicsLayerMask> _masks;

        PhysicsJoltBackend(const PhysicsJoltBackend&) = delete;
        PhysicsJoltBackend& operator=(const PhysicsJoltBackend&) = delete;
    };
} // namespace ChikaEngine::Physics
