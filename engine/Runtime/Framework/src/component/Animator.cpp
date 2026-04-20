#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/math/mat4.h"

namespace ChikaEngine::Framework
{
    void Animator::ResolveAssets(Asset::AssetManager& assetMgr, const Asset::MeshData* meshData)
    {
        if (!_isDirty)
            return;

        if (_animationClipPath.empty())
        {
            LOG_WARN("Animator", "Null of Animation Clip Path");
            return;
        }

        _animationClipHandle = assetMgr.LoadAnimationClip(_animationClipPath);
        auto* animData = assetMgr.GetAnimationClip(_animationClipHandle);

        if (!animData)
        {
            LOG_WARN("Animator", "fail to load Animation Data");
            return;
        }

        size_t jointCount = meshData->skeleton.joints.size();

        if (finalMatrices.size() != jointCount)
        {
            finalMatrices.assign(jointCount, Math::Mat4::Identity());
        }

        track2JointMap.assign(animData->tracks.size(), -1);
        for (size_t i = 0; i < animData->tracks.size(); ++i)
        {
            const std::string& jointName = animData->tracks[i].jointName;
            auto it = meshData->skeleton.jointNameMap.find(jointName);
            if (it != meshData->skeleton.jointNameMap.end())
            {
                track2JointMap[i] = it->second;
            }
        }

        _isDirty = false;
    }

    void Animator::UpdateTime(float deltatime, float duration)
    {
        if (duration <= 0.0f)
        {
            CurrentTime = 0.0f;
            return;
        }

        CurrentTime += deltatime;

        // 做最简的循环播放
        if (CurrentTime > duration)
        {
            if (IsLoop)
                CurrentTime = std::fmod(CurrentTime, duration);
            else
                CurrentTime = duration;
        }
    }

} // namespace ChikaEngine::Framework