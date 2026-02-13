#pragma once
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include <cstdint>

namespace ChikaEngine::Physics
{
    using PhysicsBodyHandle = std::uint32_t;
    using PhysicsLayerID = std::uint8_t;
    using PhysicsLayerMask = std::uint32_t; // 对应位掩码
    // 最大值说明mask了所有layer
    constexpr PhysicsLayerMask PHYSICS_LAYER_MASK_ALL = 0xFFFFFFFF;

    enum class PhysicsBackendTypes
    {
        None,
        Jolt
    };

    enum class MotionType
    {
        Static,
        Kinematic,
        Dynamic
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

    enum class ColliderShapeTypes
    {
        Box,
        Sphere
    };

    enum class RigidbodyShapes
    {
        Box,
        Sphere,
        Capsule
    };

    struct ColliderShapeDesc
    {
        RigidbodyShapes type = RigidbodyShapes::Box;
        Math::Vector3 center = {0, 0, 0}; // 相对偏移

        // Box / Sphere 参数
        Math::Vector3 halfExtents = {0.5f, 0.5f, 0.5f};
        float radius = 0.5f;
        float height = 1.0f;
    };

    struct PhysicsBodyCreateDesc
    {
        Core::GameObjectID ownerId = 0; // 用于查找 go

        // 变换 从 transform 中解耦
        Math::Vector3 position = {0, 0, 0};
        Math::Quaternion rotation = {0, 0, 0, 1};

        // 形状 (从 Collider 获取 if 有)
        ColliderShapeDesc shapeDesc;
        bool isTrigger = false;

        // 动力学属性 (来自 Rigidbody，如果没有则为默认)
        MotionType motionType = MotionType::Static;
        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.0f;

        // 当前物体在哪一层 0 是 default
        PhysicsLayerID layer = 0;
        // 需要和什么层碰撞
        PhysicsLayerMask collisionMask = PHYSICS_LAYER_MASK_ALL;
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

    // 射线信息
    struct RaycastHit
    {
        PhysicsBodyHandle bodyHandle = 0;
        float distance = 0.0f; // 距离
        Math::Vector3 point;   // 世界空间击中点
        Math::Vector3 normal;  // 世界空间击中法线
        bool hasHit = false;
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