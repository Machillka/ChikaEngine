#include "subsystem/AnimationHierarchy.hpp"
#include <cstdint>
#include <utility>

namespace ChikaEngine::Framework::Detail
{
    namespace
    {
        enum class VisitState : uint8_t
        {
            Unvisited,
            Visiting,
            Complete,
        };
    } // namespace

    SkeletonHierarchyResult ComputeSkeletonGlobalTransforms(const Asset::SkeletonData& skeleton, std::span<const Math::Mat4> localTransforms, std::vector<Math::Mat4>& globalTransforms)
    {
        const size_t jointCount = skeleton.joints.size();
        if (localTransforms.size() != jointCount)
            return SkeletonHierarchyResult::LocalTransformCountMismatch;

        for (const Asset::Joint& joint : skeleton.joints)
        {
            if (joint.parentIndex < -1 || (joint.parentIndex >= 0 && static_cast<size_t>(joint.parentIndex) >= jointCount))
                return SkeletonHierarchyResult::InvalidParentIndex;
        }

        std::vector<Math::Mat4> computedTransforms(jointCount);
        std::vector<VisitState> visitStates(jointCount, VisitState::Unvisited);

        const auto resolveJoint = [&](auto&& self, size_t index) -> SkeletonHierarchyResult
        {
            if (visitStates[index] == VisitState::Complete)
                return SkeletonHierarchyResult::Success;
            if (visitStates[index] == VisitState::Visiting)
                return SkeletonHierarchyResult::Cycle;

            visitStates[index] = VisitState::Visiting;
            const int parentIndex = skeleton.joints[index].parentIndex;
            if (parentIndex == -1)
            {
                computedTransforms[index] = localTransforms[index];
            }
            else
            {
                const SkeletonHierarchyResult parentResult = self(self, static_cast<size_t>(parentIndex));
                if (parentResult != SkeletonHierarchyResult::Success)
                    return parentResult;
                computedTransforms[index] = computedTransforms[parentIndex] * localTransforms[index];
            }
            visitStates[index] = VisitState::Complete;
            return SkeletonHierarchyResult::Success;
        };

        for (size_t index = 0; index < skeleton.joints.size(); ++index)
        {
            const SkeletonHierarchyResult result = resolveJoint(resolveJoint, index);
            if (result != SkeletonHierarchyResult::Success)
                return result;
        }

        globalTransforms = std::move(computedTransforms);
        return SkeletonHierarchyResult::Success;
    }

    std::string_view ToString(SkeletonHierarchyResult result)
    {
        switch (result)
        {
        case SkeletonHierarchyResult::Success:
            return "success";
        case SkeletonHierarchyResult::LocalTransformCountMismatch:
            return "local transform count mismatch";
        case SkeletonHierarchyResult::InvalidParentIndex:
            return "invalid parent index";
        case SkeletonHierarchyResult::Cycle:
            return "cycle";
        }
        return "unknown";
    }
} // namespace ChikaEngine::Framework::Detail
