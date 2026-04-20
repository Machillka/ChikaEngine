#include "ChikaEngine/subsystem/AnimationSubsystem.hpp"
#include "ChikaEngine/AssetAnimation.hpp"
#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/scene/scene.hpp"
namespace ChikaEngine::Framework
{
    void AnimationSubsystem::Tick(float deltaTime)
    {
        const auto& gos = _ownerScene->GetAllGameobjects();

        for (const auto& go : gos)
        {
            if (!go->IsActive())
                continue;

            auto* animator = go->GetComponent<Animator>();
            auto* meshComp = go->GetComponent<MeshRenderer>();

            if (!animator || !meshComp)
                continue;

            if (!animator->IsEnabled() || !animator->IsPlaying)
                continue;

            const auto* meshData = _assetMgr->GetMesh(meshComp->GetMeshAsset());

            if (!meshData || !meshData->isSkinned)
                continue;

            // 保证资源加载
            animator->ResolveAssets(*_assetMgr, meshData);

            const auto* animData = _assetMgr->GetAnimationClip(animator->GetAnimationClipHandle());

            if (!animData)
                continue;

            const auto& skeleton = meshData->skeleton;

            animator->CurrentTime += deltaTime * animator->PlaybackSpeed;

            animator->UpdateTime(deltaTime, animData->duration);

            std::vector<Math::Mat4> localTransforms(skeleton.joints.size());

            // 默认填充 bind pose
            for (size_t i = 0; i < skeleton.joints.size(); ++i)
            {
                localTransforms[i] = Math::Mat4::TRSMatrix(skeleton.joints[i].localBindPos, skeleton.joints[i].localBindRot, skeleton.joints[i].localBindScale);
            }

            for (size_t i = 0; i < animData->tracks.size(); ++i)
            {
                int jointIdx = animator->track2JointMap[i];
                if (jointIdx == -1)
                    continue;

                const auto& track = animData->tracks[i];
                const auto& joint = skeleton.joints[jointIdx];

                Math::Vector3 pos = track.positionKeys.empty() ? joint.localBindPos : EvalPosition(animator->CurrentTime, track.positionKeys);
                Math::Quaternion rot = track.rotationKeys.empty() ? joint.localBindRot : EvalRotation(animator->CurrentTime, track.rotationKeys);
                Math::Vector3 scale = track.scaleKeys.empty() ? joint.localBindScale : EvalScale(animator->CurrentTime, track.scaleKeys);

                localTransforms[jointIdx] = Math::Mat4::TRSMatrix(pos, rot, scale);
            }

            std::vector<Math::Mat4> globalTransforms(skeleton.joints.size());

            for (size_t i = 0; i < skeleton.joints.size(); ++i)
            {
                int parentIdx = skeleton.joints[i].parentIndex;

                if (parentIdx == -1)
                {
                    globalTransforms[i] = localTransforms[i];
                }
                else
                {
                    // NOTE: Parent 一定是先被计算好的吗？？ 还是说写成一个递归
                    globalTransforms[i] = globalTransforms[parentIdx] * localTransforms[i];
                }

                Math::Mat4 finalMat = globalTransforms[i] * skeleton.joints[i].inverseBindMat;

                animator->finalMatrices[i] = finalMat;
            }
        }
    }

    Math::Vector3 AnimationSubsystem::EvalPosition(float time, const std::vector<Asset::KeyFrame<Math::Vector3>>& keys)
    {
        if (keys.empty())
            return Math::Vector3::zero;

        if (time <= keys.front().time)
            return keys.front().val;
        if (time >= keys.back().time)
            return keys.back().val;

        for (size_t i = 0; i < keys.size() - 1; i++)
        {
            if (time >= keys[i].time && time <= keys[i + 1].time)
            {
                float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
                return Math::Vector3::Lerp(keys[i].val, keys[i + 1].val, t);
            }
        }

        return keys.back().val;
    }
    Math::Quaternion AnimationSubsystem::EvalRotation(float time, const std::vector<Asset::KeyFrame<Math::Quaternion>>& keys)
    {
        if (keys.empty())
            return Math::Quaternion::Identity();

        if (time <= keys.front().time)
            return keys.front().val;
        if (time >= keys.back().time)
            return keys.back().val;

        for (size_t i = 0; i < keys.size() - 1; i++)
        {
            if (time >= keys[i].time && time <= keys[i + 1].time)
            {
                float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
                return Math::Quaternion::Slerp(keys[i].val, keys[i + 1].val, t);
            }
        }

        return keys.back().val;
    }
    Math::Vector3 AnimationSubsystem::EvalScale(float time, const std::vector<Asset::KeyFrame<Math::Vector3>>& keys)
    {
        if (keys.empty())
            return Math::Vector3(1.0f, 1.0f, 1.0f);

        if (time <= keys.front().time)
            return keys.front().val;
        if (time >= keys.back().time)
            return keys.back().val;

        // TODO: 使用二分查找
        for (size_t i = 0; i < keys.size() - 1; i++)
        {
            if (time >= keys[i].time && time <= keys[i + 1].time)
            {
                float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
                return Math::Vector3::Lerp(keys[i].val, keys[i + 1].val, t);
            }
        }

        return keys.back().val;
    }
} // namespace ChikaEngine::Framework