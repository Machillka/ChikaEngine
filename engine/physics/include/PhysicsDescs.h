#pragma once
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
    };

    struct PhysicsSystemDesc
    {
        // 制定后端类型
        PhysicsBackendTypes backendType;

        // 默认的物理参数
        PhysicsInitDesc initDesc;
    };

    struct RigidbodyCreateDesc
    {
    };

    // 用于Collider回调
    struct CollisionEvent
    {
    };
} // namespace ChikaEngine::Physics