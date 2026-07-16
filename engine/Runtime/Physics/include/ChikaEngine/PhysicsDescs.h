#pragma once

#include "ChikaEngine/PhysicsHandles.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace ChikaEngine::Physics
{
    using PhysicsLayerID = std::uint8_t;
    using PhysicsLayerMask = std::uint32_t;

    constexpr PhysicsLayerMask PHYSICS_LAYER_MASK_ALL = 0xFFFFFFFFu;
    constexpr PhysicsLayerID PHYSICS_LAYER_COUNT = 32;

    enum class PhysicsBackendType
    {
        None,
        Jolt,
    };

    enum class PhysicsStatus
    {
        Success,
        AlreadyInitialized,
        NotInitialized,
        UnsupportedBackend,
        UnsupportedFeature,
        InvalidArgument,
        InvalidHandle,
        CapacityExceeded,
        QueueFull,
        DuplicateOwner,
        BackendFailure,
    };

    struct PhysicsResult
    {
        PhysicsStatus status = PhysicsStatus::Success;
        std::string diagnostic;

        [[nodiscard]] bool Succeeded() const noexcept
        {
            return status == PhysicsStatus::Success || status == PhysicsStatus::AlreadyInitialized;
        }

        explicit operator bool() const noexcept
        {
            return Succeeded();
        }

        [[nodiscard]] static PhysicsResult Ok()
        {
            return {};
        }

        [[nodiscard]] static PhysicsResult Failure(PhysicsStatus failureStatus, std::string message)
        {
            return PhysicsResult{ .status = failureStatus, .diagnostic = std::move(message) };
        }
    };

    enum class MotionType
    {
        Static,
        Kinematic,
        Dynamic,
    };

    enum class PhysicsWakePolicy
    {
        KeepState,
        Wake,
        DoNotWake,
    };

    enum class ColliderShapeType
    {
        Box,
        Sphere,
        Capsule,
    };

    struct PhysicsBackendCapabilities
    {
        bool boxShape = false;
        bool sphereShape = false;
        bool capsuleShape = false;
        bool closestRaycast = false;
        bool constraints = false;
        bool continuousCollisionDetection = false;

        [[nodiscard]] bool SupportsShape(ColliderShapeType type) const noexcept
        {
            switch (type)
            {
            case ColliderShapeType::Box:
                return boxShape;
            case ColliderShapeType::Sphere:
                return sphereShape;
            case ColliderShapeType::Capsule:
                return capsuleShape;
            }
            return false;
        }
    };

    /** @brief World initialization in meters, seconds, kilograms, Y-up, right-handed coordinates. */
    struct PhysicsInitDesc
    {
        Math::Vector3 gravity = Math::Vector3(0.0f, -9.81f, 0.0f);
        int workerThreadCount = -1;
    };

    struct PhysicsSystemDesc
    {
        PhysicsBackendType backendType = PhysicsBackendType::Jolt;
        PhysicsInitDesc initDesc;
        std::size_t commandQueueCapacity = 4096;
    };

    struct PhysicsTransform
    {
        Math::Vector3 pos;
        Math::Quaternion rot;
    };

    struct ColliderShapeDesc
    {
        ColliderShapeType type = ColliderShapeType::Box;
        Math::Vector3 center = { 0, 0, 0 };
        Math::Vector3 halfExtents = { 0.5f, 0.5f, 0.5f };
        float radius = 0.5f;
        float height = 1.0f;
    };

    struct PhysicsBodyCreateDesc
    {
        Core::GameObjectID ownerId = Core::InvalidGameObjectID;
        Math::Vector3 position = { 0, 0, 0 };
        Math::Quaternion rotation = { 0, 0, 0, 1 };
        ColliderShapeDesc shapeDesc;
        bool isTrigger = false;
        MotionType motionType = MotionType::Dynamic;
        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.0f;
        PhysicsLayerID layer = 0;
        PhysicsLayerMask collisionMask = PHYSICS_LAYER_MASK_ALL;
    };

    struct PhysicsBodyCreateResult
    {
        PhysicsResult result = PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics backend is not initialized");
        PhysicsBodyHandle handle = PhysicsBodyHandle::Invalid();

        [[nodiscard]] bool Succeeded() const noexcept
        {
            return result.Succeeded() && handle.IsValid();
        }

        explicit operator bool() const noexcept
        {
            return Succeeded();
        }
    };

    struct CollisionEvent
    {
        PhysicsBodyHandle selfRigidbodyHandle = PhysicsBodyHandle::Invalid();
        PhysicsBodyHandle otherRigidbodyHandle = PhysicsBodyHandle::Invalid();
        Math::Vector3 contactPoint;
        Math::Vector3 contactNormal;
        float impulse = 0.0f;
    };

    struct Ray
    {
        Math::Vector3 origin;
        Math::Vector3 direction;
    };

    struct RaycastHit
    {
        PhysicsBodyHandle bodyHandle = PhysicsBodyHandle::Invalid();
        Core::GameObjectID gameObjectId = Core::InvalidGameObjectID;
        float distance = 0.0f;
        Math::Vector3 point;
        Math::Vector3 normal;
        bool hasHit = false;
    };

} // namespace ChikaEngine::Physics
