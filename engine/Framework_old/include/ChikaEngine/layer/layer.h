#pragma once
#include "ChikaEngine/PhysicsDescs.h"
#include <cstdint>
#include <initializer_list>
namespace ChikaEngine::Framework
{

    // 最大支持 32 层
    using LayerMask = std::uint32_t;

    enum class GameObjectLayer : std::uint8_t
    {
        Default = 0,
        Player = 1,
        Enemy = 2,
        Projectile = 3,
        Trigger = 4,
        Count // 末尾插眼
    };

    // 把 枚举转成 bitmask
    inline LayerMask LayerBit(GameObjectLayer layer)
    {
        return (LayerMask(1) << static_cast<std::uint32_t>(layer));
    }

    // 合并多层
    inline LayerMask MakeMask(std::initializer_list<GameObjectLayer> list)
    {
        LayerMask m = 0;
        for (auto l : list)
            m |= LayerBit(l);
        return m;
    }

    inline Physics::PhysicsLayerID ToPhysicsLayer(GameObjectLayer layer)
    {
        return static_cast<Physics::PhysicsLayerID>(layer);
    }

    // 将 framework 的 Mask 转为 physics 的 Mask
    inline Physics::PhysicsLayerMask ToPhysicsMask(LayerMask mask)
    {
        return static_cast<Physics::PhysicsLayerMask>(mask);
    }

    // 反向转化
    inline GameObjectLayer ToGameLayer(Physics::PhysicsLayerID physicsLayer)
    {
        return static_cast<GameObjectLayer>(physicsLayer);
    }
} // namespace ChikaEngine::Framework