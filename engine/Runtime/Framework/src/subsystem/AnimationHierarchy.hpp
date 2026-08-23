#pragma once

#include "ChikaEngine/AssetAnimation.hpp"
#include <span>
#include <string_view>
#include <vector>

namespace ChikaEngine::Framework::Detail
{
    enum class SkeletonHierarchyResult
    {
        Success,
        LocalTransformCountMismatch,
        InvalidParentIndex,
        Cycle,
    };

    [[nodiscard]] SkeletonHierarchyResult ComputeSkeletonGlobalTransforms(const Asset::SkeletonData& skeleton, std::span<const Math::Mat4> localTransforms, std::vector<Math::Mat4>& globalTransforms);
    [[nodiscard]] std::string_view ToString(SkeletonHierarchyResult result);
} // namespace ChikaEngine::Framework::Detail
