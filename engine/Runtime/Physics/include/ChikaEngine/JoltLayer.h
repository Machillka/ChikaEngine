#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "Jolt/Core/Core.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"

#include <cstdint>
#include <vector>

// 提供给 Jolt 的辅助函数
namespace ChikaEngine::Physics::JoltHelper
{
    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);

        static constexpr JPH::uint NUM_LAYERS = 2;
    }; // namespace BroadPhaseLayers

    inline JPH::ObjectLayer GetJoltObjectLayer(std::uint32_t gameLayerIndex, MotionType motion)
    {
        bool isMoving = (motion != MotionType::Static);
        return (JPH::ObjectLayer)((gameLayerIndex << 1) | (isMoving ? 1 : 0));
    }

    // 从 Jolt ObjectLayer 解码出 GameLayerIndex
    inline std::uint32_t GetGameLayerIndex(JPH::ObjectLayer layer)
    {
        return (std::uint32_t)(layer >> 1);
    }

    class BitmaskBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
    {
      public:
        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch ((JPH::BroadPhaseLayer::Type)inLayer)
            {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
                return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
                return "MOVING";
            default:
                return "INVALID";
            }
        }
#endif

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            // 检查最低位
            if (inLayer & 1)
            {
                return BroadPhaseLayers::MOVING;
            }
            return BroadPhaseLayers::NON_MOVING;
        }
    };

    class BitmaskObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
      public:
        // 1: 物体当前所在的层级 2: 碰撞对方所在层级
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            bool isDynamic1 = (inLayer1 & 1) != 0;

            // 动态物体需要和所有东西碰撞 (静态 & 动态)
            if (isDynamic1)
            {
                return true;
            }
            else
            {
                // 静态物体 只需要和 动态层 碰撞
                return inLayer2 == BroadPhaseLayers::MOVING;
            }
        }
    };
    class BitmaskObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
    {
      public:
        explicit BitmaskObjectLayerPairFilter(const std::vector<Framework::LayerMask>& masks) : _masks(masks) {}
        void SetMask(std::uint32_t layerIndex, Framework::LayerMask mask)
        {
            if (layerIndex >= _masks.size())
                _masks.resize(layerIndex + 1, ~Framework::LayerMask(0));
            _masks[layerIndex] = mask;
        }
        bool ShouldCollide(JPH::ObjectLayer inA, JPH::ObjectLayer inB) const override
        {
            std::uint32_t a = GetGameLayerIndex(inA);
            std::uint32_t b = GetGameLayerIndex(inB);
            if (a >= _masks.size() || b >= _masks.size())
                return true;
            return (_masks[a] & (Framework::LayerMask(1) << b)) != 0;
        }

      private:
        std::vector<Framework::LayerMask> _masks;
    };

} // namespace ChikaEngine::Physics::JoltHelper