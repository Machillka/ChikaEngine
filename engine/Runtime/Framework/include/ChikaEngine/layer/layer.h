#pragma once
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
} // namespace ChikaEngine::Framework