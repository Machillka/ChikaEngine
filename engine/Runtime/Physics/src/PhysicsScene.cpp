#include "ChikaEngine/PhysicsScene.h"
#include "PhysicsJoltBackend.hpp"
#include "ChikaEngine/debug/log_macros.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ChikaEngine::Physics
{
    PhysicsScene::PhysicsScene(const PhysicsSystemDesc& desc)
    {
        _initializationResult = Initialize(desc);
    }

    PhysicsScene::~PhysicsScene()
    {
        Shutdown();
    }

    PhysicsResult PhysicsScene::Initialize(const PhysicsSystemDesc& desc)
    {
        if (IsInitialized())
        {
            _initializationResult = PhysicsResult{ .status = PhysicsStatus::AlreadyInitialized, .diagnostic = "Physics scene is already initialized" };
            return _initializationResult;
        }

        PhysicsResult commandBufferResult = _commandBuffer.Configure(desc.commandQueueCapacity);
        if (!commandBufferResult)
        {
            _initializationResult = std::move(commandBufferResult);
            return _initializationResult;
        }

        std::unique_ptr<IPhysicsBackend> candidate;
        switch (desc.backendType)
        {
        case PhysicsBackendType::None:
            _initializationResult = PhysicsResult::Failure(PhysicsStatus::UnsupportedBackend, "Physics backend 'None' cannot create a simulation world");
            return _initializationResult;
        case PhysicsBackendType::Jolt:
            candidate = std::make_unique<PhysicsJoltBackend>();
            break;
        }

        if (!candidate)
        {
            _initializationResult = PhysicsResult::Failure(PhysicsStatus::UnsupportedBackend, "Unknown physics backend");
            return _initializationResult;
        }

        PhysicsResult result = candidate->Initialize(desc.initDesc);
        if (!result)
        {
            candidate->Shutdown();
            _initializationResult = std::move(result);
            return _initializationResult;
        }

        _backend = std::move(candidate);
        _initializationResult = std::move(result);
        return _initializationResult;
    }

    void PhysicsScene::Shutdown() noexcept
    {
        ResetSceneState();
        if (_backend)
        {
            _backend->Shutdown();
            _backend.reset();
        }
        _initializationResult = PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
    }

    void PhysicsScene::ResetSceneState() noexcept
    {
        (void)_commandBuffer.Clear();
        if (_backend)
        {
            _backend->ClearBodies();
            (void)_backend->PollCollisionEvents();
        }
        _bodyRegistry.Clear();
        _updatedTransforms.clear();
        std::lock_guard lock(_statisticsMutex);
        _lastExecutionTrace.clear();
    }

    bool PhysicsScene::IsInitialized() const noexcept
    {
        return _backend && _backend->IsInitialized();
    }

    const PhysicsResult& PhysicsScene::GetInitializationResult() const noexcept
    {
        return _initializationResult;
    }

    PhysicsBackendCapabilities PhysicsScene::GetCapabilities() const noexcept
    {
        return _backend ? _backend->GetCapabilities() : PhysicsBackendCapabilities{};
    }

    bool PhysicsScene::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit)
    {
        outHit = {};
        if (!IsInitialized())
            return false;

        if (_backend->Raycast(origin, direction, maxDistance, outHit))
        {
            const auto record = _bodyRegistry.Find(outHit.bodyHandle);
            if (record)
            {
                outHit.gameObjectId = record->ownerId;
                return true;
            }
        }
        outHit = {};
        return false;
    }

    bool PhysicsScene::EnqueueRigidbodyDestroy(PhysicsBodyHandle handle)
    {
        return static_cast<bool>(QueueDestroyBody(handle));
    }

    PhysicsResult PhysicsScene::QueueCreateBody(const PhysicsBodyCreateDesc& desc)
    {
        if (!IsInitialized())
            return PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
        if (!Core::IsValidGameObjectID(desc.ownerId))
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics body owner is invalid");
        return _commandBuffer.Enqueue(PhysicsCreateCommand{ .desc = desc });
    }

    PhysicsResult PhysicsScene::QueueRebuildBody(const PhysicsBodyCreateDesc& desc)
    {
        if (!IsInitialized())
            return PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
        if (!Core::IsValidGameObjectID(desc.ownerId))
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics body owner is invalid");
        return _commandBuffer.Enqueue(PhysicsRebuildCommand{ .desc = desc });
    }

    PhysicsResult PhysicsScene::QueueDestroyBody(PhysicsBodyHandle handle)
    {
        if (!IsInitialized())
            return PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
        if (!_bodyRegistry.Find(handle))
        {
            std::lock_guard lock(_statisticsMutex);
            ++_staleCommands;
            return PhysicsResult::Failure(PhysicsStatus::InvalidHandle, "Physics body handle is stale or belongs to another Scene");
        }
        return _commandBuffer.Enqueue(PhysicsDestroyCommand{ .target = PhysicsBodyTarget::FromHandle(handle) });
    }

    PhysicsResult PhysicsScene::QueueDestroyBody(Core::GameObjectID ownerId)
    {
        if (!IsInitialized())
            return PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
        if (!Core::IsValidGameObjectID(ownerId))
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics body owner is invalid");
        return _commandBuffer.Enqueue(PhysicsDestroyCommand{ .target = PhysicsBodyTarget::FromOwner(ownerId) });
    }

    PhysicsResult PhysicsScene::QueueTeleport(PhysicsBodyTarget target, const Math::Vector3& position, const Math::Quaternion& rotation, bool resetVelocity, PhysicsWakePolicy wakePolicy)
    {
        if (!IsInitialized())
            return PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
        if (target.handle && !_bodyRegistry.Find(target.handle))
        {
            std::lock_guard lock(_statisticsMutex);
            ++_staleCommands;
            return PhysicsResult::Failure(PhysicsStatus::InvalidHandle, "Physics body handle is stale or belongs to another Scene");
        }
        if (!target.handle && !Core::IsValidGameObjectID(target.ownerId))
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics command target is invalid");
        return _commandBuffer.Enqueue(PhysicsTeleportCommand{ .target = target, .position = position, .rotation = rotation, .resetVelocity = resetVelocity, .wakePolicy = wakePolicy });
    }

    PhysicsResult PhysicsScene::QueueKinematicTarget(PhysicsBodyTarget target, const Math::Vector3& position, const Math::Quaternion& rotation)
    {
        if (!IsInitialized())
            return PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
        if (target.handle && !_bodyRegistry.Find(target.handle))
        {
            std::lock_guard lock(_statisticsMutex);
            ++_staleCommands;
            return PhysicsResult::Failure(PhysicsStatus::InvalidHandle, "Physics body handle is stale or belongs to another Scene");
        }
        if (!target.handle && !Core::IsValidGameObjectID(target.ownerId))
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics command target is invalid");
        return _commandBuffer.Enqueue(PhysicsKinematicTargetCommand{ .target = target, .position = position, .rotation = rotation });
    }

    PhysicsResult PhysicsScene::QueueSetLinearVelocity(PhysicsBodyTarget target, const Math::Vector3& velocity)
    {
        if (!IsInitialized())
            return PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
        if (target.handle && !_bodyRegistry.Find(target.handle))
        {
            std::lock_guard lock(_statisticsMutex);
            ++_staleCommands;
            return PhysicsResult::Failure(PhysicsStatus::InvalidHandle, "Physics body handle is stale or belongs to another Scene");
        }
        if (!target.handle && !Core::IsValidGameObjectID(target.ownerId))
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics command target is invalid");
        return _commandBuffer.Enqueue(PhysicsVelocityCommand{ .target = target, .velocity = velocity });
    }

    PhysicsResult PhysicsScene::QueueAddForce(PhysicsBodyTarget target, const Math::Vector3& force, PhysicsWakePolicy wakePolicy)
    {
        if (!IsInitialized())
            return PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
        if (target.handle && !_bodyRegistry.Find(target.handle))
        {
            std::lock_guard lock(_statisticsMutex);
            ++_staleCommands;
            return PhysicsResult::Failure(PhysicsStatus::InvalidHandle, "Physics body handle is stale or belongs to another Scene");
        }
        if (!target.handle && !Core::IsValidGameObjectID(target.ownerId))
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics command target is invalid");
        return _commandBuffer.Enqueue(PhysicsForceCommand{ .target = target, .force = force, .wakePolicy = wakePolicy });
    }

    PhysicsResult PhysicsScene::QueueApplyImpulse(PhysicsBodyTarget target, const Math::Vector3& impulse)
    {
        if (!IsInitialized())
            return PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
        if (target.handle && !_bodyRegistry.Find(target.handle))
        {
            std::lock_guard lock(_statisticsMutex);
            ++_staleCommands;
            return PhysicsResult::Failure(PhysicsStatus::InvalidHandle, "Physics body handle is stale or belongs to another Scene");
        }
        if (!target.handle && !Core::IsValidGameObjectID(target.ownerId))
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics command target is invalid");
        return _commandBuffer.Enqueue(PhysicsImpulseCommand{ .target = target, .impulse = impulse });
    }

    void PhysicsScene::Tick(float fixedDeltaTime)
    {
        if (!IsInitialized())
            return;
        PreStep(fixedDeltaTime);
        (void)Simulate(fixedDeltaTime);
    }

    void PhysicsScene::PreStep(float fixedDeltaTime)
    {
        std::vector<PhysicsCommand> commands = _commandBuffer.Drain();
        {
            std::lock_guard lock(_statisticsMutex);
            _lastExecutionTrace.clear();
        }
        if (commands.empty())
            return;

        std::vector<bool> retained(commands.size(), true);
        std::unordered_map<Core::GameObjectID, std::size_t> latestStructuralCommand;
        // Structural commands are final-state intents. Keeping only the last
        // command per owner handles Awake/Enable/Dirty/Destroy combinations.
        for (std::size_t index = 0; index < commands.size(); ++index)
        {
            const auto owner = GetStructuralOwner(commands[index]);
            if (!owner)
                continue;
            const auto [it, inserted] = latestStructuralCommand.emplace(*owner, index);
            if (!inserted)
            {
                retained[it->second] = false;
                it->second = index;
                std::lock_guard lock(_statisticsMutex);
                ++_coalescedCommands;
            }
        }

        std::array<std::vector<const PhysicsCommand*>, 4> phases;
        // Phase ordering is part of the public lifecycle contract. Commands
        // retain enqueue order inside each phase.
        for (std::size_t index = 0; index < commands.size(); ++index)
        {
            if (!retained[index])
                continue;
            const PhysicsCommandType type = GetPhysicsCommandType(commands[index].payload);
            std::size_t phase = 3;
            if (type == PhysicsCommandType::Destroy)
                phase = 0;
            else if (type == PhysicsCommandType::Create || type == PhysicsCommandType::Rebuild)
                phase = 1;
            else if (type == PhysicsCommandType::Teleport || type == PhysicsCommandType::KinematicTarget)
                phase = 2;
            phases[phase].push_back(&commands[index]);
        }

        for (const auto& phase : phases)
        {
            for (const PhysicsCommand* command : phase)
            {
                const PhysicsResult result = ExecuteCommand(*command, fixedDeltaTime);
                RecordExecution(*command, result);
            }
        }
    }

    bool PhysicsScene::Simulate(float fixedDeltaTime)
    {
        return IsInitialized() && _backend->Simulate(fixedDeltaTime);
    }

    const std::vector<std::pair<Core::GameObjectID, PhysicsTransform>>& PhysicsScene::PollTransform()
    {
        _updatedTransforms.clear();
        if (!IsInitialized())
            return _updatedTransforms;

        for (const PhysicsBodyRecord& record : _bodyRegistry.SnapshotActive())
        {
            PhysicsTransform ts;
            if (_backend->TrySyncTransform(record.backendToken, ts))
                _updatedTransforms.emplace_back(record.ownerId, ts);
        }

        return _updatedTransforms;
    }

    bool PhysicsScene::SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3& velocity)
    {
        return static_cast<bool>(QueueSetLinearVelocity(PhysicsBodyTarget::FromHandle(handle), velocity));
    }

    bool PhysicsScene::ApplyForce(PhysicsBodyHandle handle, const Math::Vector3& force, PhysicsWakePolicy wakePolicy)
    {
        return static_cast<bool>(QueueAddForce(PhysicsBodyTarget::FromHandle(handle), force, wakePolicy));
    }

    bool PhysicsScene::ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3& impulse)
    {
        return static_cast<bool>(QueueApplyImpulse(PhysicsBodyTarget::FromHandle(handle), impulse));
    }

    bool PhysicsScene::SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& position, const Math::Quaternion& rotation)
    {
        return static_cast<bool>(QueueTeleport(PhysicsBodyTarget::FromHandle(handle), position, rotation));
    }

    bool PhysicsScene::SetKinematicTarget(PhysicsBodyHandle handle, const Math::Vector3& position, const Math::Quaternion& rotation)
    {
        return static_cast<bool>(QueueKinematicTarget(PhysicsBodyTarget::FromHandle(handle), position, rotation));
    }

    bool PhysicsScene::SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask)
    {
        return IsInitialized() && _backend->SetLayerCollisionMask(layerId, mask);
    }

    PhysicsLayerMask PhysicsScene::GetLayerCollisionMask(PhysicsLayerID layerId) const
    {
        if (IsInitialized())
            return _backend->GetLayerCollisionMask(layerId);
        return 0;
    }

    PhysicsBodyCreateResult PhysicsScene::CreateBodyImmediate(const PhysicsBodyCreateDesc& desc)
    {
        if (!IsInitialized())
            return { .result = PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized") };
        return CreateBodyInternal(desc);
    }

    PhysicsBodyCreateResult PhysicsScene::CreateBodyInternal(const PhysicsBodyCreateDesc& desc)
    {
        if (!Core::IsValidGameObjectID(desc.ownerId))
            return { .result = PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics body owner is invalid") };
        if (_bodyRegistry.FindByOwner(desc.ownerId))
            return { .result = PhysicsResult::Failure(PhysicsStatus::DuplicateOwner, "Physics body owner already has an active body") };

        const PhysicsBodyHandle handle = _bodyRegistry.Reserve();
        if (!handle)
            return { .result = PhysicsResult::Failure(PhysicsStatus::CapacityExceeded, "Physics body registry is exhausted") };

        const PhysicsBackendBodyCreateResult backendResult = _backend->CreateBodyFromDesc(handle, desc);
        if (!backendResult)
        {
            (void)_bodyRegistry.CancelReservation(handle);
            LOG_ERROR("Physics", "Failed to create body for owner {}: {}", desc.ownerId, backendResult.result.diagnostic);
            return { .result = backendResult.result };
        }

        const PhysicsBodyRecord record{
            .handle = handle,
            .backendToken = backendResult.token,
            .ownerId = desc.ownerId,
            .colliderHandle = PhysicsColliderHandle::Invalid(),
            .motionType = desc.motionType,
            .active = true,
        };
        if (!_bodyRegistry.Commit(record))
        {
            (void)_backend->DestroyPhysicsBody(backendResult.token);
            (void)_bodyRegistry.CancelReservation(handle);
            return { .result = PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Failed to commit physics body registry record") };
        }

        return { .result = PhysicsResult::Ok(), .handle = handle };
    }

    PhysicsResult PhysicsScene::RebuildBodyInternal(const PhysicsBodyCreateDesc& desc)
    {
        const auto oldRecord = _bodyRegistry.FindByOwner(desc.ownerId);
        if (!oldRecord)
            return CreateBodyInternal(desc).result;

        const PhysicsBodyHandle newHandle = _bodyRegistry.Reserve();
        if (!newHandle)
            return PhysicsResult::Failure(PhysicsStatus::CapacityExceeded, "Physics body registry is exhausted");

        const PhysicsBackendBodyCreateResult backendResult = _backend->CreateBodyFromDesc(newHandle, desc);
        if (!backendResult)
        {
            (void)_bodyRegistry.CancelReservation(newHandle);
            return backendResult.result;
        }

        // The old record remains authoritative until create-new succeeds.
        // If retiring old fails, roll back the new backend Body and reservation.
        if (!_backend->DestroyPhysicsBody(oldRecord->backendToken))
        {
            (void)_backend->DestroyPhysicsBody(backendResult.token);
            (void)_bodyRegistry.CancelReservation(newHandle);
            return PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Failed to retire the previous physics body during rebuild");
        }

        const PhysicsBodyRecord replacement{
            .handle = newHandle,
            .backendToken = backendResult.token,
            .ownerId = desc.ownerId,
            .colliderHandle = oldRecord->colliderHandle,
            .motionType = desc.motionType,
            .active = true,
        };
        if (!_bodyRegistry.Replace(oldRecord->handle, replacement))
        {
            (void)_backend->DestroyPhysicsBody(backendResult.token);
            (void)_bodyRegistry.CancelReservation(newHandle);
            (void)_bodyRegistry.Remove(oldRecord->handle);
            return PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Failed to replace the physics body registry record");
        }

        return PhysicsResult::Ok();
    }

    PhysicsResult PhysicsScene::DestroyBodyInternal(const PhysicsDestroyCommand& command)
    {
        const auto record = ResolveTarget(command.target);
        if (!record)
        {
            if (!command.target.handle && Core::IsValidGameObjectID(command.target.ownerId))
                return PhysicsResult::Ok();
            return PhysicsResult::Failure(PhysicsStatus::InvalidHandle, "Physics destroy command references a stale body");
        }

        if (!_backend->DestroyPhysicsBody(record->backendToken))
            return PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Physics backend failed to destroy body");
        if (!_bodyRegistry.Remove(record->handle))
            return PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Physics registry failed to remove destroyed body");
        return PhysicsResult::Ok();
    }

    std::optional<PhysicsBodyRecord> PhysicsScene::ResolveTarget(PhysicsBodyTarget target) const
    {
        if (target.handle)
            return _bodyRegistry.Find(target.handle);
        if (Core::IsValidGameObjectID(target.ownerId))
            return _bodyRegistry.FindByOwner(target.ownerId);
        return std::nullopt;
    }

    std::optional<Core::GameObjectID> PhysicsScene::GetStructuralOwner(const PhysicsCommand& command) const
    {
        return std::visit(
            [this](const auto& payload) -> std::optional<Core::GameObjectID>
            {
                using Command = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<Command, PhysicsCreateCommand> || std::is_same_v<Command, PhysicsRebuildCommand>)
                {
                    if (Core::IsValidGameObjectID(payload.desc.ownerId))
                        return payload.desc.ownerId;
                }
                else if constexpr (std::is_same_v<Command, PhysicsDestroyCommand>)
                {
                    if (Core::IsValidGameObjectID(payload.target.ownerId))
                        return payload.target.ownerId;
                    const auto record = _bodyRegistry.Find(payload.target.handle);
                    if (record)
                        return record->ownerId;
                }
                return std::nullopt;
            },
            command.payload);
    }

    PhysicsResult PhysicsScene::ExecuteCommand(const PhysicsCommand& command, float fixedDeltaTime)
    {
        return std::visit(
            [this, fixedDeltaTime](const auto& payload) -> PhysicsResult
            {
                using Command = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<Command, PhysicsCreateCommand>)
                {
                    return CreateBodyInternal(payload.desc).result;
                }
                else if constexpr (std::is_same_v<Command, PhysicsDestroyCommand>)
                {
                    return DestroyBodyInternal(payload);
                }
                else if constexpr (std::is_same_v<Command, PhysicsRebuildCommand>)
                {
                    return RebuildBodyInternal(payload.desc);
                }
                else
                {
                    const auto record = ResolveTarget(payload.target);
                    if (!record)
                        return PhysicsResult::Failure(PhysicsStatus::InvalidHandle, "Physics command references a stale body");

                    bool succeeded = false;
                    if constexpr (std::is_same_v<Command, PhysicsTeleportCommand>)
                    {
                        succeeded = _backend->TeleportBody(record->backendToken, payload.position, payload.rotation, payload.resetVelocity, payload.wakePolicy);
                    }
                    else if constexpr (std::is_same_v<Command, PhysicsKinematicTargetCommand>)
                    {
                        if (record->motionType != MotionType::Kinematic)
                            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Kinematic target requires a kinematic body");
                        succeeded = _backend->SetKinematicTarget(record->backendToken, payload.position, payload.rotation, fixedDeltaTime);
                    }
                    else if constexpr (std::is_same_v<Command, PhysicsVelocityCommand>)
                    {
                        if (record->motionType == MotionType::Static)
                            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Static body cannot receive velocity commands");
                        succeeded = _backend->SetLinearVelocity(record->backendToken, payload.velocity);
                    }
                    else if constexpr (std::is_same_v<Command, PhysicsForceCommand>)
                    {
                        if (record->motionType != MotionType::Dynamic)
                            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Force requires a dynamic body");
                        succeeded = _backend->AddForce(record->backendToken, payload.force, payload.wakePolicy);
                    }
                    else if constexpr (std::is_same_v<Command, PhysicsImpulseCommand>)
                    {
                        if (record->motionType != MotionType::Dynamic)
                            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Impulse requires a dynamic body");
                        succeeded = _backend->ApplyImpulse(record->backendToken, payload.impulse);
                    }

                    return succeeded ? PhysicsResult::Ok() : PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Physics backend command failed");
                }
            },
            command.payload);
    }

    void PhysicsScene::RecordExecution(const PhysicsCommand& command, const PhysicsResult& result)
    {
        std::lock_guard lock(_statisticsMutex);
        ++_processedCommands;
        if (!result)
            ++_failedCommands;
        if (result.status == PhysicsStatus::InvalidHandle)
            ++_staleCommands;
        _lastExecutionTrace.push_back(PhysicsCommandExecutionRecord{
            .sequence = command.sequence,
            .type = GetPhysicsCommandType(command.payload),
            .status = result.status,
        });
    }

    bool PhysicsScene::HasBody(PhysicsBodyHandle handle) const
    {
        if (!IsInitialized())
            return false;
        const auto record = _bodyRegistry.Find(handle);
        return record && _backend->HasBody(record->backendToken);
    }

    PhysicsBodyHandle PhysicsScene::GetBodyHandle(Core::GameObjectID ownerId) const
    {
        const auto record = _bodyRegistry.FindByOwner(ownerId);
        return record ? record->handle : PhysicsBodyHandle::Invalid();
    }

    std::optional<PhysicsBodyRecord> PhysicsScene::GetBodyRecord(PhysicsBodyHandle handle) const
    {
        return _bodyRegistry.Find(handle);
    }

    PhysicsSceneStatistics PhysicsScene::GetStatistics() const
    {
        const PhysicsCommandBufferStatistics commandStats = _commandBuffer.GetStatistics();
        const std::size_t activeBodies = _bodyRegistry.ActiveCount();
        const std::size_t backendBodies = _backend ? _backend->GetBodyCount() : 0;
        std::lock_guard lock(_statisticsMutex);
        return PhysicsSceneStatistics{
            .commandCapacity = commandStats.capacity,
            .pendingCommands = commandStats.pendingCommands,
            .peakPendingCommands = commandStats.peakPendingCommands,
            .activeBodies = activeBodies,
            .backendBodies = backendBodies,
            .submittedCommands = commandStats.enqueuedCommands,
            .processedCommands = _processedCommands,
            .failedCommands = _failedCommands + commandStats.rejectedCommands,
            .staleCommands = _staleCommands,
            .coalescedCommands = _coalescedCommands,
            .clearedCommands = commandStats.clearedCommands,
        };
    }

    std::vector<PhysicsCommandExecutionRecord> PhysicsScene::GetLastCommandExecutionTrace() const
    {
        std::lock_guard lock(_statisticsMutex);
        return _lastExecutionTrace;
    }
} // namespace ChikaEngine::Physics
