#pragma once

#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include <string>
#include <unordered_map>
#include <vector>
namespace ChikaEngine::Asset
{
    struct Joint
    {
        std::string name;
        int parentIndex = -1; // 指向父节点的下标, 其中 -1 为 root

        // bind 下的 local 坐标系变换
        Math::Vector3 localBindPos = Math::Vector3::zero;
        Math::Quaternion localBindRot = Math::Quaternion::Identity();
        Math::Vector3 localBindScale = Math::Vector3(1.0f, 1.0f, 1.0f);

        // IBM
        Math::Mat4 inverseBindMat = Math::Mat4::Identity();
    };

    struct SkeletonData
    {
        std::vector<Joint> joints;
        // TODO: 优化
        std::unordered_map<std::string, int> jointNameMap;
    };

    // 泛型关键帧
    template <typename T> struct KeyFrame
    {
        float time;
        T val;
    };

    struct AnimationTrack
    {
        std::string jointName; // 骨骼节点名字

        std::vector<KeyFrame<Math::Vector3>> positionKeys;    // 位置
        std::vector<KeyFrame<Math::Quaternion>> rotationKeys; // 旋转
        std::vector<KeyFrame<Math::Vector3>> scaleKeys;       // 缩放
    };

    struct AnimationClipData
    {
        std::string name;
        float duration = 0.0f;
        std::vector<AnimationTrack> tracks;
    };
} // namespace ChikaEngine::Asset