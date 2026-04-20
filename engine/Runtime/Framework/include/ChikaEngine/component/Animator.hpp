#pragma once

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"
#include "ChikaEngine/AssetHandle.hpp"
#include <string>
#include <vector>

namespace ChikaEngine::Asset
{
    class AssetManager;
}

namespace ChikaEngine::Framework
{

    MCLASS(Animator) : public Component
    {
        REFLECTION_BODY(Animator);

      public:
        Animator()
        {
            SetReflectedClassName("Animator");
        }

        explicit Animator(const std::string& animationClipPath) : _animationClipPath(animationClipPath), _isDirty(true) {}

        ~Animator() override = default;

        /*!
         * @brief  Set the Target object
         *
         * @param  meshPath
         * @param  animationClipPath
         * @author Machillka (machillka2007@gmail.com)
         * @date 2026-04-18
         */
        void ResolveAssets(Asset::AssetManager & assetMgr, const Asset::MeshData* meshData);

        Asset::AnimationClipHandle GetAnimationClipHandle() const
        {
            return _animationClipHandle;
        }

        void UpdateTime(float deltatime, float duration);

      public:
        // TODO: 写成属性访问器, 懒惰导致
        float CurrentTime = 0.0f;
        float PlaybackSpeed = 1.0;
        bool IsPlaying = true;
        bool IsLoop = true;
        std::vector<int> track2JointMap;       // 缓存动画 Track idx ->  Joint Index
        std::vector<Math::Mat4> finalMatrices; // 存储结果, 由 render subsystem 直接 push 到 render 层

      private:
        MFIELD()
        std::string _animationClipPath;

        Asset::AnimationClipHandle _animationClipHandle{};

        // float _currentTime = 0.0f;
        // bool _isPlaying = true;
        // float _playbackSpeed = 1.0f;
        // bool _isLoop = true;

        bool _isDirty = true; // 标记动画数据需要更新
    };
} // namespace ChikaEngine::Framework