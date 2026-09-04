#pragma once

#include "ChikaEngine/IPhysicsBackend.h"
#include "ChikaEngine/PhysicsDescs.h"

#include <Jolt/Core/Core.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace ChikaEngine::Physics::JoltHelper
{
    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint NUM_LAYERS = 2;
    } // namespace BroadPhaseLayers

    inline JPH::ObjectLayer GetJoltObjectLayer(PhysicsLayerID gameLayerIndex, MotionType motion)
    {
        const bool isMoving = motion != MotionType::Static;
        return static_cast<JPH::ObjectLayer>((gameLayerIndex << 1u) | (isMoving ? 1u : 0u));
    }

    inline PhysicsLayerID GetGameLayerIndex(JPH::ObjectLayer layer)
    {
        return static_cast<PhysicsLayerID>(layer >> 1u);
    }

    inline bool IsMoving(JPH::ObjectLayer layer)
    {
        return (layer & 1u) != 0;
    }

    class BitmaskBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
    {
      public:
        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer))
            {
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING):
                return "NON_MOVING";
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):
                return "MOVING";
            default:
                return "INVALID";
            }
        }
#endif

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            return IsMoving(inLayer) ? BroadPhaseLayers::MOVING : BroadPhaseLayers::NON_MOVING;
        }
    };

    class BitmaskObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
      public:
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            if (IsMoving(inLayer1))
                return true;
            return inLayer2 == BroadPhaseLayers::MOVING;
        }
    };

    class BitmaskObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
    {
      public:
        explicit BitmaskObjectLayerPairFilter(const IPhysicsBackend* backend) : _backend(backend) {}

        bool ShouldCollide(JPH::ObjectLayer inA, JPH::ObjectLayer inB) const override
        {
            const PhysicsLayerID layerA = GetGameLayerIndex(inA);
            const PhysicsLayerID layerB = GetGameLayerIndex(inB);
            const PhysicsLayerMask maskA = _backend->GetLayerCollisionMask(layerA);
            const PhysicsLayerMask maskB = _backend->GetLayerCollisionMask(layerB);
            const bool aHitsB = (maskA & (PhysicsLayerMask(1) << layerB)) != 0;
            const bool bHitsA = (maskB & (PhysicsLayerMask(1) << layerA)) != 0;
            return aHitsB && bHitsA;
        }

      private:
        const IPhysicsBackend* _backend = nullptr;
    };
} // namespace ChikaEngine::Physics::JoltHelper
