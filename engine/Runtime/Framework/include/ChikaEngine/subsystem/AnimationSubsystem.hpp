#pragma once

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/subsystem/ISubsystem.h"
#include "ChikaEngine/AssetAnimation.hpp"
#include <vector>

namespace ChikaEngine::Framework
{
    class Scene;

    class AnimationSubsystem : public ISubsystem
    {
      public:
        AnimationSubsystem(Scene* ownerScene, Asset::AssetManager* assetMgr) : _ownerScene(ownerScene), _assetMgr(assetMgr) {}
        ~AnimationSubsystem() override = default;

        void Tick(float deltaTime) override;
        void Cleanup() override {};

      private:
        Math::Vector3 EvalPosition(float time, const std::vector<Asset::KeyFrame<Math::Vector3>>& keys);
        Math::Quaternion EvalRotation(float time, const std::vector<Asset::KeyFrame<Math::Quaternion>>& keys);
        Math::Vector3 EvalScale(float time, const std::vector<Asset::KeyFrame<Math::Vector3>>& keys);
        void ComputeGlobalTransform(int i, const Asset::SkeletonData& skeleton, const std::vector<Math::Mat4>& locals, std::vector<Math::Mat4>& globals, std::vector<bool>& computed);

      private:
        Scene* _ownerScene = nullptr;
        Asset::AssetManager* _assetMgr = nullptr;
    };
} // namespace ChikaEngine::Framework