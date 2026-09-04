#pragma once

#include "ChikaEngine/PhysicsEvents.hpp"

#include <cstdint>

namespace ChikaEngine::Framework
{
    /**
     * @brief Value-only owner-local projection of one canonical physics pair event.
     *
     * `normal` and `relativeVelocity` are oriented from self toward other.
     * Alive flags describe the exact Body/GameObject participant represented by
     * the event handles, not merely whether a GameObject with the same ID exists.
     */
    struct PhysicsContactEvent
    {
        Physics::PhysicsPairPhase phase = Physics::PhysicsPairPhase::Enter;
        Physics::PhysicsPairKind kind = Physics::PhysicsPairKind::Collision;
        Physics::PhysicsBodyHandle selfBody = Physics::PhysicsBodyHandle::Invalid();
        Physics::PhysicsBodyHandle otherBody = Physics::PhysicsBodyHandle::Invalid();
        Physics::PhysicsColliderHandle selfCollider = Physics::PhysicsColliderHandle::Invalid();
        Physics::PhysicsColliderHandle otherCollider = Physics::PhysicsColliderHandle::Invalid();
        Core::GameObjectID selfGameObject = Core::InvalidGameObjectID;
        Core::GameObjectID otherGameObject = Core::InvalidGameObjectID;
        Physics::PhysicsContactData contact;
        Physics::PhysicsPairTerminationReason terminationReason = Physics::PhysicsPairTerminationReason::None;
        bool hasContactData = false;
        bool selfAlive = false;
        bool otherAlive = false;
        std::uint64_t fixedStepIndex = 0;
    };
} // namespace ChikaEngine::Framework
