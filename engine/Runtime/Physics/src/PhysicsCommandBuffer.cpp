#include "ChikaEngine/PhysicsCommandBuffer.hpp"

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <utility>

namespace ChikaEngine::Physics
{
    PhysicsCommandBuffer::PhysicsCommandBuffer(std::size_t capacity) : _capacity(std::max<std::size_t>(capacity, 1))
    {
        _commands.reserve(_capacity);
    }

    PhysicsResult PhysicsCommandBuffer::Configure(std::size_t capacity)
    {
        if (capacity == 0)
            return PhysicsResult::Failure(PhysicsStatus::InvalidArgument, "Physics command queue capacity must be greater than zero");

        std::lock_guard lock(_mutex);
        if (!_commands.empty())
            return PhysicsResult::Failure(PhysicsStatus::BackendFailure, "Cannot reconfigure a non-empty physics command buffer");

        _capacity = capacity;
        _commands.reserve(_capacity);
        return PhysicsResult::Ok();
    }

    PhysicsResult PhysicsCommandBuffer::Enqueue(PhysicsCommandPayload payload)
    {
        std::lock_guard lock(_mutex);
        if (_commands.size() >= _capacity)
        {
            ++_rejectedCommands;
            return PhysicsResult::Failure(PhysicsStatus::QueueFull, "Physics command queue is full");
        }

        _commands.push_back(PhysicsCommand{ .sequence = _nextSequence++, .payload = std::move(payload) });
        ++_enqueuedCommands;
        _peakPendingCommands = std::max(_peakPendingCommands, _commands.size());
        return PhysicsResult::Ok();
    }

    std::vector<PhysicsCommand> PhysicsCommandBuffer::Drain()
    {
        std::lock_guard lock(_mutex);
        std::vector<PhysicsCommand> drained;
        drained.reserve(_commands.size());
        std::move(_commands.begin(), _commands.end(), std::back_inserter(drained));
        _commands.clear();
        return drained;
    }

    std::size_t PhysicsCommandBuffer::Clear() noexcept
    {
        std::lock_guard lock(_mutex);
        const std::size_t count = _commands.size();
        _commands.clear();
        _clearedCommands += count;
        return count;
    }

    PhysicsCommandBufferStatistics PhysicsCommandBuffer::GetStatistics() const
    {
        std::lock_guard lock(_mutex);
        return PhysicsCommandBufferStatistics{
            .capacity = _capacity,
            .pendingCommands = _commands.size(),
            .peakPendingCommands = _peakPendingCommands,
            .enqueuedCommands = _enqueuedCommands,
            .rejectedCommands = _rejectedCommands,
            .clearedCommands = _clearedCommands,
        };
    }

    PhysicsCommandType GetPhysicsCommandType(const PhysicsCommandPayload& payload) noexcept
    {
        return std::visit(
            [](const auto& command)
            {
                using Command = std::decay_t<decltype(command)>;
                if constexpr (std::is_same_v<Command, PhysicsCreateCommand>)
                    return PhysicsCommandType::Create;
                else if constexpr (std::is_same_v<Command, PhysicsDestroyCommand>)
                    return PhysicsCommandType::Destroy;
                else if constexpr (std::is_same_v<Command, PhysicsRebuildCommand>)
                    return PhysicsCommandType::Rebuild;
                else if constexpr (std::is_same_v<Command, PhysicsTeleportCommand>)
                    return PhysicsCommandType::Teleport;
                else if constexpr (std::is_same_v<Command, PhysicsKinematicTargetCommand>)
                    return PhysicsCommandType::KinematicTarget;
                else if constexpr (std::is_same_v<Command, PhysicsVelocityCommand>)
                    return PhysicsCommandType::Velocity;
                else if constexpr (std::is_same_v<Command, PhysicsAngularVelocityCommand>)
                    return PhysicsCommandType::AngularVelocity;
                else if constexpr (std::is_same_v<Command, PhysicsForceCommand>)
                    return PhysicsCommandType::Force;
                else if constexpr (std::is_same_v<Command, PhysicsTorqueCommand>)
                    return PhysicsCommandType::Torque;
                else if constexpr (std::is_same_v<Command, PhysicsImpulseCommand>)
                    return PhysicsCommandType::Impulse;
                else if constexpr (std::is_same_v<Command, PhysicsAngularImpulseCommand>)
                    return PhysicsCommandType::AngularImpulse;
                else
                    return PhysicsCommandType::Activation;
            },
            payload);
    }
} // namespace ChikaEngine::Physics
