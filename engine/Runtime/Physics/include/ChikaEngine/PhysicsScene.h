#pragma once

#include "ChikaEngine/IPhysicsBackend.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/base/UIDGenerator.h"

#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ChikaEngine::Physics
{
    /** @brief Owns one backend-independent physics world for one Framework Scene. */
    class PhysicsScene
    {
      public:
        explicit PhysicsScene(const PhysicsSystemDesc& desc);
        ~PhysicsScene();

        PhysicsScene(const PhysicsScene&) = delete;
        PhysicsScene& operator=(const PhysicsScene&) = delete;

        [[nodiscard]] PhysicsResult Initialize(const PhysicsSystemDesc& desc);
        void Shutdown() noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] const PhysicsResult& GetInitializationResult() const noexcept;
        [[nodiscard]] PhysicsBackendCapabilities GetCapabilities() const noexcept;

        void Tick(float fixedDeltaTime);

        [[nodiscard]] bool EnqueueRigidbodyDestroy(PhysicsBodyHandle handle);
        [[nodiscard]] bool SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3& velocity);
        [[nodiscard]] bool ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3& impulse);
        [[nodiscard]] bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit);
        [[nodiscard]] PhysicsBodyCreateResult CreateBodyImmediate(const PhysicsBodyCreateDesc& desc);
        [[nodiscard]] bool SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& pos, const Math::Quaternion& rot);
        [[nodiscard]] bool HasBody(PhysicsBodyHandle handle) const;

        [[nodiscard]] const std::vector<std::pair<Core::GameObjectID, PhysicsTransform>>& PollTransform();

        [[nodiscard]] bool SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask);
        [[nodiscard]] PhysicsLayerMask GetLayerCollisionMask(PhysicsLayerID layerId) const;

      private:
        void ProcessDestroyRigidbodyQueue();
        void RegisterRigidbody(PhysicsBodyHandle handle, Core::GameObjectID id);

        std::unique_ptr<IPhysicsBackend> _backend;
        PhysicsResult _initializationResult = PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene has not been initialized");

        std::mutex _createRigidbodyMutex;
        std::mutex _destroyRigidbodyMutex;
        std::queue<PhysicsBodyHandle> _destroyRigidbodyQueue;

        std::unordered_map<PhysicsBodyHandle, Core::GameObjectID, PhysicsHandleHash> _physicsHandleToGO;
        std::vector<std::pair<Core::GameObjectID, PhysicsTransform>> _updatedTransforms;
    };
} // namespace ChikaEngine::Physics
