#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "Jolt/Core/Core.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"
#include "IPhysicsBackend.h"

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

    inline JPH::ObjectLayer GetJoltObjectLayer(PhysicsLayerID gameLayerIndex, MotionType motion)
    {
        bool isMoving = (motion != MotionType::Static);
        return static_cast<JPH::ObjectLayer>((gameLayerIndex << 1) | (isMoving ? 1 : 0));
    }

    // 从 Jolt ObjectLayer 解码出 GameLayerIndex
    inline PhysicsLayerID GetGameLayerIndex(JPH::ObjectLayer layer)
    {
        return static_cast<PhysicsLayerID>(layer >> 1);
    }

    inline bool IsMoving(JPH::ObjectLayer layer)
    {
        return (layer & 1) != 0;
    }

    // 物体去哪个 BP 层
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
            if (IsMoving(inLayer))
                return BroadPhaseLayers::MOVING;
            return BroadPhaseLayers::NON_MOVING;
        }
    };

    // 物体能否和 BP 层碰撞
    class BitmaskObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
      public:
        // 1: 物体当前所在的层级 2: 碰撞对方所在层级
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            if (IsMoving(inLayer1))
                return true;

            // 静态物体 只需要和 动态层 碰撞
            return inLayer2 == BroadPhaseLayers::MOVING;
        }
    };

    // 逻辑层碰撞矩阵检测
    class BitmaskObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
    {
      public:
        // 持有 Backend 指针以实时获取 Mask
        explicit BitmaskObjectLayerPairFilter(const IPhysicsBackend* backend) : _backend(backend) {}

        bool ShouldCollide(JPH::ObjectLayer inA, JPH::ObjectLayer inB) const override
        {
            PhysicsLayerID layerA = GetGameLayerIndex(inA);
            PhysicsLayerID layerB = GetGameLayerIndex(inB);

            PhysicsLayerMask maskA = _backend->GetLayerCollisionMask(layerA);
            PhysicsLayerMask maskB = _backend->GetLayerCollisionMask(layerB);

            // 检查 A 是否允许撞 B
            bool aHitsB = (maskA & (PhysicsLayerMask(1) << layerB)) != 0;
            // 检查 B 是否允许撞 A
            bool bHitsA = (maskB & (PhysicsLayerMask(1) << layerA)) != 0;

            return aHitsB && bHitsA;
        }

      private:
        // 使用指针获得实时更新的 mask 数据
        const IPhysicsBackend* _backend;
    };

} // namespace ChikaEngine::Physics::JoltHelper