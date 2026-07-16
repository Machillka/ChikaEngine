#pragma once

#include "ChikaEngine/IPhysicsBackend.h"
#include "ChikaEngine/PhysicsBodyRegistry.hpp"
#include "ChikaEngine/PhysicsCommandBuffer.hpp"
#include "ChikaEngine/PhysicsDescs.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace ChikaEngine::Physics
{
    struct PhysicsCommandExecutionRecord
    {
        std::uint64_t sequence = 0;
        PhysicsCommandType type = PhysicsCommandType::Create;
        PhysicsStatus status = PhysicsStatus::Success;
    };

    struct PhysicsSceneStatistics
    {
        std::size_t commandCapacity = 0;
        std::size_t pendingCommands = 0;
        std::size_t peakPendingCommands = 0;
        std::size_t activeBodies = 0;
        std::size_t backendBodies = 0;
        std::uint64_t submittedCommands = 0;
        std::uint64_t processedCommands = 0;
        std::uint64_t failedCommands = 0;
        std::uint64_t staleCommands = 0;
        std::uint64_t coalescedCommands = 0;
        std::uint64_t clearedCommands = 0;
    };

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
        void ResetSceneState() noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] const PhysicsResult& GetInitializationResult() const noexcept;
        [[nodiscard]] PhysicsBackendCapabilities GetCapabilities() const noexcept;

        void Tick(float fixedDeltaTime);
        void PreStep(float fixedDeltaTime);
        [[nodiscard]] bool Simulate(float fixedDeltaTime);

        [[nodiscard]] PhysicsResult QueueCreateBody(const PhysicsBodyCreateDesc& desc);
        [[nodiscard]] PhysicsResult QueueRebuildBody(const PhysicsBodyCreateDesc& desc);
        [[nodiscard]] PhysicsResult QueueDestroyBody(PhysicsBodyHandle handle);
        [[nodiscard]] PhysicsResult QueueDestroyBody(Core::GameObjectID ownerId);
        [[nodiscard]] PhysicsResult QueueTeleport(PhysicsBodyTarget target, const Math::Vector3& position, const Math::Quaternion& rotation, bool resetVelocity = true, PhysicsWakePolicy wakePolicy = PhysicsWakePolicy::Wake);
        [[nodiscard]] PhysicsResult QueueKinematicTarget(PhysicsBodyTarget target, const Math::Vector3& position, const Math::Quaternion& rotation);
        [[nodiscard]] PhysicsResult QueueSetLinearVelocity(PhysicsBodyTarget target, const Math::Vector3& velocity);
        [[nodiscard]] PhysicsResult QueueAddForce(PhysicsBodyTarget target, const Math::Vector3& force, PhysicsWakePolicy wakePolicy = PhysicsWakePolicy::Wake);
        [[nodiscard]] PhysicsResult QueueApplyImpulse(PhysicsBodyTarget target, const Math::Vector3& impulse);

        [[nodiscard]] bool EnqueueRigidbodyDestroy(PhysicsBodyHandle handle);
        [[nodiscard]] bool SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3& velocity);
        [[nodiscard]] bool ApplyForce(PhysicsBodyHandle handle, const Math::Vector3& force, PhysicsWakePolicy wakePolicy = PhysicsWakePolicy::Wake);
        [[nodiscard]] bool ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3& impulse);
        [[nodiscard]] bool SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& position, const Math::Quaternion& rotation);
        [[nodiscard]] bool SetKinematicTarget(PhysicsBodyHandle handle, const Math::Vector3& position, const Math::Quaternion& rotation);

        [[nodiscard]] bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit);

        /** @warning Restricted to initialization and tests. Runtime gameplay should use QueueCreateBody/QueueRebuildBody. */
        [[nodiscard]] PhysicsBodyCreateResult CreateBodyImmediate(const PhysicsBodyCreateDesc& desc);

        [[nodiscard]] bool HasBody(PhysicsBodyHandle handle) const;
        [[nodiscard]] PhysicsBodyHandle GetBodyHandle(Core::GameObjectID ownerId) const;
        [[nodiscard]] std::optional<PhysicsBodyRecord> GetBodyRecord(PhysicsBodyHandle handle) const;
        [[nodiscard]] const std::vector<std::pair<Core::GameObjectID, PhysicsTransform>>& PollTransform();

        [[nodiscard]] bool SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask);
        [[nodiscard]] PhysicsLayerMask GetLayerCollisionMask(PhysicsLayerID layerId) const;

        [[nodiscard]] PhysicsSceneStatistics GetStatistics() const;
        [[nodiscard]] std::vector<PhysicsCommandExecutionRecord> GetLastCommandExecutionTrace() const;

      private:
        [[nodiscard]] PhysicsBodyCreateResult CreateBodyInternal(const PhysicsBodyCreateDesc& desc);
        [[nodiscard]] PhysicsResult RebuildBodyInternal(const PhysicsBodyCreateDesc& desc);
        [[nodiscard]] PhysicsResult DestroyBodyInternal(const PhysicsDestroyCommand& command);
        [[nodiscard]] PhysicsResult ExecuteCommand(const PhysicsCommand& command, float fixedDeltaTime);
        [[nodiscard]] std::optional<PhysicsBodyRecord> ResolveTarget(PhysicsBodyTarget target) const;
        [[nodiscard]] std::optional<Core::GameObjectID> GetStructuralOwner(const PhysicsCommand& command) const;
        void RecordExecution(const PhysicsCommand& command, const PhysicsResult& result);

        std::unique_ptr<IPhysicsBackend> _backend;
        PhysicsResult _initializationResult = PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene has not been initialized");
        PhysicsCommandBuffer _commandBuffer;
        PhysicsBodyRegistry _bodyRegistry;

        std::vector<std::pair<Core::GameObjectID, PhysicsTransform>> _updatedTransforms;

        mutable std::mutex _statisticsMutex;
        std::uint64_t _processedCommands = 0;
        std::uint64_t _failedCommands = 0;
        std::uint64_t _staleCommands = 0;
        std::uint64_t _coalescedCommands = 0;
        std::vector<PhysicsCommandExecutionRecord> _lastExecutionTrace;
    };
} // namespace ChikaEngine::Physics
