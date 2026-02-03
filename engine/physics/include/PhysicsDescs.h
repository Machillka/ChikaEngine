#pragma once
#include "framework/gameobject/GameObject.h"
#include "math/quaternion.h"
#include "math/vector3.h"

#include <cstdint>
namespace ChikaEngine::Physics
{
    using PhysicsBodyHandle = std::uint32_t;

    enum class PhysicsBackendTypes
    {
        None,
        Jolt
    };

    // 提供默认的物理参数
    struct PhysicsInitDesc
    {
        // Default gravity: downwards on Y
        Math::Vector3 gravity = Math::Vector3(0.0f, -9.8f, 0.0f);
    };

    struct PhysicsSystemDesc
    {
        // 制定后端类型
        PhysicsBackendTypes backendType;

        // 默认的物理参数
        PhysicsInitDesc initDesc;
    };

    // 用于记录物理计算后的位置
    struct PhysicsTransform
    {
        Math::Vector3 pos;
        Math::Quaternion rot;
    };

    enum class RigidbodyShapes
    {
        Box,
        Sphare
    };
    struct RigidbodyCreateDesc
    {
        RigidbodyShapes shape = RigidbodyShapes::Box;
        PhysicsTransform transform;
        Math::Vector3 halfExtents{0.5f, 0.5f, 0.5f};
        float radius = 0.5f;
        float mass = 1.0f;
        bool isKinematic = false;
        Framework::GameObjectID ownerId = 0; // 用于查找 go
    };

    // 用于Collider回调
    struct CollisionEvent
    {
        PhysicsBodyHandle selfRigidbodyHandle;
        PhysicsBodyHandle oherRigidbodyHandle;
        Math::Vector3 contactPoint;
        Math::Vector3 contactNormal;
        float impulse;
    };

    // 处理修改速度事件的数据
    struct VelocityCommand
    {
        PhysicsBodyHandle handle;
        Math::Vector3 v;
    };

    // 处理冲量的数据
    struct ImpulseCommand
    {
        PhysicsBodyHandle handle;
        Math::Vector3 impulse;
    };
} // namespace ChikaEngine::Physics