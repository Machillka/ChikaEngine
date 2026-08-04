#pragma once

#include "ChikaEngine/PhysicsDescs.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <variant>
#include <vector>

namespace ChikaEngine::Physics
{
    struct PhysicsBodyTarget
    {
        PhysicsBodyHandle handle = PhysicsBodyHandle::Invalid();
        Core::GameObjectID ownerId = Core::InvalidGameObjectID;

        [[nodiscard]] static PhysicsBodyTarget FromHandle(PhysicsBodyHandle value) noexcept
        {
            return PhysicsBodyTarget{ .handle = value };
        }

        [[nodiscard]] static PhysicsBodyTarget FromOwner(Core::GameObjectID value) noexcept
        {
            return PhysicsBodyTarget{ .ownerId = value };
        }
    };

    struct PhysicsCreateCommand
    {
        PhysicsBodyCreateDesc desc;
    };

    struct PhysicsDestroyCommand
    {
        PhysicsBodyTarget target;
    };

    struct PhysicsRebuildCommand
    {
        PhysicsBodyCreateDesc desc;
    };

    struct PhysicsTeleportCommand
    {
        PhysicsBodyTarget target;
        Math::Vector3 position;
        Math::Quaternion rotation = Math::Quaternion::Identity();
        bool resetVelocity = true;
        PhysicsWakePolicy wakePolicy = PhysicsWakePolicy::Wake;
    };

    struct PhysicsKinematicTargetCommand
    {
        PhysicsBodyTarget target;
        Math::Vector3 position;
        Math::Quaternion rotation = Math::Quaternion::Identity();
    };

    struct PhysicsVelocityCommand
    {
        PhysicsBodyTarget target;
        Math::Vector3 velocity;
    };

    struct PhysicsAngularVelocityCommand
    {
        PhysicsBodyTarget target;
        Math::Vector3 velocity;
    };

    struct PhysicsForceCommand
    {
        PhysicsBodyTarget target;
        Math::Vector3 force;
        PhysicsWakePolicy wakePolicy = PhysicsWakePolicy::Wake;
    };

    struct PhysicsImpulseCommand
    {
        PhysicsBodyTarget target;
        Math::Vector3 impulse;
    };

    struct PhysicsTorqueCommand
    {
        PhysicsBodyTarget target;
        Math::Vector3 torque;
        PhysicsWakePolicy wakePolicy = PhysicsWakePolicy::Wake;
    };

    struct PhysicsAngularImpulseCommand
    {
        PhysicsBodyTarget target;
        Math::Vector3 impulse;
    };

    struct PhysicsActivationCommand
    {
        PhysicsBodyTarget target;
        bool activate = true;
    };

    using PhysicsCommandPayload = std::variant<PhysicsCreateCommand, PhysicsDestroyCommand, PhysicsRebuildCommand, PhysicsTeleportCommand, PhysicsKinematicTargetCommand, PhysicsVelocityCommand, PhysicsAngularVelocityCommand, PhysicsForceCommand, PhysicsTorqueCommand, PhysicsImpulseCommand, PhysicsAngularImpulseCommand, PhysicsActivationCommand>;

    enum class PhysicsCommandType
    {
        Create,
        Destroy,
        Rebuild,
        Teleport,
        KinematicTarget,
        Velocity,
        AngularVelocity,
        Force,
        Torque,
        Impulse,
        AngularImpulse,
        Activation,
    };

    struct PhysicsCommand
    {
        std::uint64_t sequence = 0;
        PhysicsCommandPayload payload;
    };

    struct PhysicsCommandBufferStatistics
    {
        std::size_t capacity = 0;
        std::size_t pendingCommands = 0;
        std::size_t peakPendingCommands = 0;
        std::uint64_t enqueuedCommands = 0;
        std::uint64_t rejectedCommands = 0;
        std::uint64_t clearedCommands = 0;
    };

    /** @brief Thread-safe fixed-capacity producer queue drained once at a PhysicsScene PreStep. */
    class PhysicsCommandBuffer
    {
      public:
        explicit PhysicsCommandBuffer(std::size_t capacity = 4096);

        [[nodiscard]] PhysicsResult Configure(std::size_t capacity);
        [[nodiscard]] PhysicsResult Enqueue(PhysicsCommandPayload payload);
        [[nodiscard]] std::vector<PhysicsCommand> Drain();
        std::size_t Clear() noexcept;
        [[nodiscard]] PhysicsCommandBufferStatistics GetStatistics() const;

      private:
        mutable std::mutex _mutex;
        std::vector<PhysicsCommand> _commands;
        std::size_t _capacity = 4096;
        std::size_t _peakPendingCommands = 0;
        std::uint64_t _nextSequence = 1;
        std::uint64_t _enqueuedCommands = 0;
        std::uint64_t _rejectedCommands = 0;
        std::uint64_t _clearedCommands = 0;
    };

    [[nodiscard]] PhysicsCommandType GetPhysicsCommandType(const PhysicsCommandPayload& payload) noexcept;
} // namespace ChikaEngine::Physics
