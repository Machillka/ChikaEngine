#pragma once

#include "ChikaEngine/PhysicsHandles.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/math/vector3.h"

#include <cstdint>

namespace ChikaEngine::Physics
{
    enum class RawContactPhase : std::uint8_t
    {
        Added,
        Persisted,
        Removed,
    };

    enum class RawContactRemovalState : std::uint8_t
    {
        NotApplicable,
        Separated,
        OtherContactActive,
        Deactivated,
        BodyMissing,
    };

    enum class PhysicsPairPhase : std::uint8_t
    {
        Enter,
        Stay,
        Exit,
    };

    enum class PhysicsPairKind : std::uint8_t
    {
        Collision,
        Trigger,
    };

    enum class PhysicsPairTerminationReason : std::uint8_t
    {
        None,
        Separated,
        BodyDestroyed,
        FilterChanged,
    };

    /**
     * @brief Backend-neutral contact sample with explicit data validity.
     *
     * Jolt reports Added/Persisted before the solver has produced a final
     * impulse. `hasImpulse` therefore remains false until a future post-solve
     * provider can supply a real value; zero is never used to mean unknown.
     */
    struct PhysicsContactData
    {
        Math::Vector3 point;
        Math::Vector3 normal;
        Math::Vector3 relativeVelocity;
        float penetration = 0.0f;
        float impulse = 0.0f;
        bool hasPoint = false;
        bool hasNormal = false;
        bool hasPenetration = false;
        bool hasRelativeVelocity = false;
        bool hasImpulse = false;

        [[nodiscard]] bool HasContactData() const noexcept
        {
            return hasPoint || hasNormal || hasPenetration;
        }
    };

    struct PhysicsContactFeatureKey
    {
        std::uint32_t featureA = 0;
        std::uint32_t featureB = 0;

        [[nodiscard]] constexpr bool operator==(const PhysicsContactFeatureKey&) const noexcept = default;
        [[nodiscard]] constexpr bool operator<(const PhysicsContactFeatureKey& other) const noexcept
        {
            if (featureA != other.featureA)
                return featureA < other.featureA;
            return featureB < other.featureB;
        }
    };

    /** @brief POD packet copied by a backend contact callback and enriched post-step. */
    struct RawContactPacket
    {
        RawContactPhase phase = RawContactPhase::Added;
        PhysicsBodyHandle bodyA = PhysicsBodyHandle::Invalid();
        PhysicsBodyHandle bodyB = PhysicsBodyHandle::Invalid();
        PhysicsColliderHandle colliderA = PhysicsColliderHandle::Invalid();
        PhysicsColliderHandle colliderB = PhysicsColliderHandle::Invalid();
        PhysicsContactFeatureKey feature;
        PhysicsContactData contact;
        RawContactRemovalState removalState = RawContactRemovalState::NotApplicable;
        bool isSensorPair = false;
        bool bodyAExists = false;
        bool bodyBExists = false;
        bool bodyAActive = false;
        bool bodyBActive = false;
        std::uint64_t fixedStepIndex = 0;
        std::uint64_t sequence = 0;
    };

    /** @brief Sorted engine identity for a backend-independent contact pair. */
    struct PhysicsPairKey
    {
        PhysicsBodyHandle bodyA = PhysicsBodyHandle::Invalid();
        PhysicsBodyHandle bodyB = PhysicsBodyHandle::Invalid();
        PhysicsColliderHandle colliderA = PhysicsColliderHandle::Invalid();
        PhysicsColliderHandle colliderB = PhysicsColliderHandle::Invalid();

        [[nodiscard]] constexpr bool operator==(const PhysicsPairKey&) const noexcept = default;
        [[nodiscard]] constexpr bool operator<(const PhysicsPairKey& other) const noexcept
        {
            if (bodyA.Value() != other.bodyA.Value())
                return bodyA.Value() < other.bodyA.Value();
            if (colliderA.Value() != other.colliderA.Value())
                return colliderA.Value() < other.colliderA.Value();
            if (bodyB.Value() != other.bodyB.Value())
                return bodyB.Value() < other.bodyB.Value();
            return colliderB.Value() < other.colliderB.Value();
        }
    };

    /**
     * @brief One canonical Scene event. Framework projects this into A/B
     * self-oriented gameplay views, then publishes this value once to observers.
     */
    struct PhysicsPairEvent
    {
        PhysicsPairPhase phase = PhysicsPairPhase::Enter;
        PhysicsPairKind kind = PhysicsPairKind::Collision;
        PhysicsPairKey pair;
        Core::GameObjectID gameObjectA = Core::InvalidGameObjectID;
        Core::GameObjectID gameObjectB = Core::InvalidGameObjectID;
        PhysicsContactData contact;
        PhysicsPairTerminationReason terminationReason = PhysicsPairTerminationReason::None;
        bool hasContactData = false;
        std::uint64_t fixedStepIndex = 0;
    };
} // namespace ChikaEngine::Physics
