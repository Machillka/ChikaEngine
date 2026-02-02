#pragma once

#include "Jolt/Core/Core.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"
#include "debug/assert.h"
#include "framework/layer/layer.h"
#include <cstdint>
#include <vector>

// 提供给 Jolt 的辅助函数
namespace ChikaEngine::Physics::JoltHelper
{
    class BitmaskBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
    {
      public:
        BitmaskBroadPhaseLayerInterface(std::uint32_t numLayers = 1) : _numLayers(numLayers)
        {
            CHIKA_ASSERT(_numLayers > 0);
        };

        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return _numLayers;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override
        {
            // 全部设置到默认上
            return JPH::BroadPhaseLayer(0);
        }

      private:
        JPH::uint _numLayers;
    };

    class BitmaskObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
      public:
        bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override
        {
            return true;
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
            uint32_t a = inA;
            uint32_t b = inB;
            if (a >= _masks.size() || b >= _masks.size())
                return true;
            return (_masks[a] & (Framework::LayerMask(1) << b)) != 0;
        }

      private:
        std::vector<Framework::LayerMask> _masks;
    };

} // namespace ChikaEngine::Physics::JoltHelper