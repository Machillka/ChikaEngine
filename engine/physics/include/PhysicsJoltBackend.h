#pragma once

#include "IPhysicsBackend.h"
#include "PhysicsDescs.h"
#include "framework/layer/layer.h"
#include "math/vector3.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

// 声明 减少头文件依赖
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

    class PhysicsJoltBackend : public IPhysicsBackend
    {
      public:
        PhysicsJoltBackend();
        ~PhysicsJoltBackend();
        bool Initialize(const PhysicsInitDesc& desc) override;
        void Shutdown() override;
        void Simulate(float fixedDeltaTime) override;
        PhysicsBodyHandle CreateBodyFromDesc(const RigidbodyCreateDesc& desc) override;
        void DestroyPhysicsBody(PhysicsBodyHandle handle) override;
        bool TrySyncTransform(PhysicsBodyHandle handle, PhysicsTransform& trans) override;
        std::vector<CollisionEvent> PollCollisionEvents() override;
        void SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3 v) override;
        void ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3 impulse) override;

        void SetLayerMask(std::uint32_t layerIndex, Framework::LayerMask mask) override;
        Framework::LayerMask GetLayerMask(std::uint32_t layerIndex) const override;

        bool HasRigidbody(PhysicsBodyHandle handle) override;

      private:
        // Jolt objects (owned)
        std::unique_ptr<JPH::TempAllocatorImpl> _tempAllocator;
        std::unique_ptr<JPH::JobSystemThreadPool> _jobSystem;
        std::unique_ptr<JPH::PhysicsSystem> _physicsSystem;

        JPH::BodyInterface* _bodyInterface = nullptr;

        JPH::Ref<JPH::Shape> CreateShape(const RigidbodyCreateDesc& desc);

        std::mutex _eventMutex;
        std::vector<CollisionEvent> _eventQueue;
        // 声明 碰撞event的监听 可以拥有push事件的权限
        class JoltBackendContactListener;
        std::unique_ptr<JoltBackendContactListener> _listener = nullptr;
        void PushEvent(const CollisionEvent& e);

        std::unique_ptr<JPH::BroadPhaseLayerInterface> _bpInterface;
        std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> _objVsBPFilter;
        std::unique_ptr<JPH::ObjectLayerPairFilter> _pairFilter;

        mutable std::mutex _maskMutex;
        std::vector<ChikaEngine::Framework::LayerMask> _masks;

        mutable std::mutex _commandMutex;
        std::vector<VelocityCommand> _velocityCommands;
        std::vector<ImpulseCommand> _impulseCommands;

        PhysicsJoltBackend(const PhysicsJoltBackend&) = delete;
        PhysicsJoltBackend& operator=(const PhysicsJoltBackend&) = delete;
    };
} // namespace ChikaEngine::Physics