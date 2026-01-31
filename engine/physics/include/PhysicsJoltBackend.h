#pragma once

#include "IPhysicsBackend.h"
#include "PhysicsDescs.h"
#include "component/Transform.h"

#include <mutex>
#include <unordered_map>

// 声明 防止依赖
namespace JPH
{
    class PhysicsSystem;
    class BodyInterface;
    class TempAllocatorImpl;
    class JobSystemThreadPool;
    class Shape;
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
        void Simulate(float dt) override;
        PhysicsBodyHandle CreateBodyFromDesc(const RigidbodyCreateDesc& desc) override;
        void DestroyPhysicsBody(PhysicsBodyHandle handle) override;
        bool TrySyncTransform(PhysicsBodyHandle handle, Framework::Transform& trans) override;
        std::vector<CollisionEvent> PollCollisionEvents() override;

      private:
        JPH::TempAllocatorImpl* mTempAllocator = nullptr;
        JPH::JobSystemThreadPool* mJobSystem = nullptr;
        JPH::PhysicsSystem* mPhysicsSystem = nullptr;
        JPH::BodyInterface* mBodyInterface = nullptr;
        std::mutex _mapMutex;
    };
} // namespace ChikaEngine::Physics