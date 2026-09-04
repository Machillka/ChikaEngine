#include "subsystem/AnimationHierarchy.hpp"
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    using ChikaEngine::Asset::SkeletonData;
    using ChikaEngine::Framework::Detail::ComputeSkeletonGlobalTransforms;
    using ChikaEngine::Framework::Detail::SkeletonHierarchyResult;
    using ChikaEngine::Math::Mat4;

    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    bool MatricesEqual(const Mat4& lhs, const Mat4& rhs)
    {
        for (size_t index = 0; index < lhs.m.size(); ++index)
        {
            if (std::abs(lhs.m[index] - rhs.m[index]) > 0.00001f)
                return false;
        }
        return true;
    }

    Mat4 Translation(float x, float y, float z)
    {
        return Mat4::TRSMatrix({ x, y, z }, ChikaEngine::Math::Quaternion::Identity(), { 1.0f, 1.0f, 1.0f });
    }

    void TestParentStoredAfterChild()
    {
        SkeletonData skeleton;
        skeleton.joints.resize(2);
        skeleton.joints[0].parentIndex = 1;
        skeleton.joints[1].parentIndex = -1;

        const std::vector localTransforms{ Translation(0.0f, 2.0f, 0.0f), Translation(10.0f, 0.0f, 0.0f) };
        std::vector<Mat4> globalTransforms;
        const auto result = ComputeSkeletonGlobalTransforms(skeleton, localTransforms, globalTransforms);

        Check(result == SkeletonHierarchyResult::Success, "parent-after-child hierarchy succeeds");
        Check(globalTransforms.size() == 2, "parent-after-child hierarchy returns every joint");
        if (globalTransforms.size() == 2)
        {
            Check(MatricesEqual(globalTransforms[0], Translation(10.0f, 2.0f, 0.0f)), "child uses the later parent's global transform");
            Check(MatricesEqual(globalTransforms[1], Translation(10.0f, 0.0f, 0.0f)), "later parent keeps its root transform");
        }
    }

    void TestMultipleRoots()
    {
        SkeletonData skeleton;
        skeleton.joints.resize(4);
        skeleton.joints[0].parentIndex = -1;
        skeleton.joints[1].parentIndex = 0;
        skeleton.joints[2].parentIndex = -1;
        skeleton.joints[3].parentIndex = 2;

        const std::vector localTransforms{
            Translation(1.0f, 0.0f, 0.0f),
            Translation(0.0f, 2.0f, 0.0f),
            Translation(0.0f, 0.0f, 3.0f),
            Translation(4.0f, 0.0f, 0.0f),
        };
        std::vector<Mat4> globalTransforms;
        const auto result = ComputeSkeletonGlobalTransforms(skeleton, localTransforms, globalTransforms);

        Check(result == SkeletonHierarchyResult::Success, "multiple-root hierarchy succeeds");
        Check(globalTransforms.size() == 4, "multiple-root hierarchy returns every joint");
        if (globalTransforms.size() == 4)
        {
            Check(MatricesEqual(globalTransforms[0], Translation(1.0f, 0.0f, 0.0f)), "first root keeps its local transform");
            Check(MatricesEqual(globalTransforms[1], Translation(1.0f, 2.0f, 0.0f)), "first root child inherits its parent");
            Check(MatricesEqual(globalTransforms[2], Translation(0.0f, 0.0f, 3.0f)), "second root keeps its local transform");
            Check(MatricesEqual(globalTransforms[3], Translation(4.0f, 0.0f, 3.0f)), "second root child inherits its parent");
        }
    }

    void TestInvalidParentIndex()
    {
        SkeletonData skeleton;
        skeleton.joints.resize(2);
        skeleton.joints[0].parentIndex = -1;
        skeleton.joints[1].parentIndex = 2;

        const std::vector localTransforms{ Translation(1.0f, 0.0f, 0.0f), Translation(0.0f, 2.0f, 0.0f) };
        const std::vector<Mat4> originalOutput{ Translation(42.0f, 0.0f, 0.0f) };
        std::vector globalTransforms = originalOutput;
        const auto result = ComputeSkeletonGlobalTransforms(skeleton, localTransforms, globalTransforms);

        Check(result == SkeletonHierarchyResult::InvalidParentIndex, "out-of-range parent index is rejected");
        Check(globalTransforms.size() == originalOutput.size() && MatricesEqual(globalTransforms[0], originalOutput[0]), "invalid parent does not publish partial transforms");

        skeleton.joints[1].parentIndex = -2;
        globalTransforms = originalOutput;
        const auto negativeResult = ComputeSkeletonGlobalTransforms(skeleton, localTransforms, globalTransforms);
        Check(negativeResult == SkeletonHierarchyResult::InvalidParentIndex, "parent index below the root sentinel is rejected");
        Check(globalTransforms.size() == originalOutput.size() && MatricesEqual(globalTransforms[0], originalOutput[0]), "negative invalid parent does not publish partial transforms");
    }

    void TestParentCycle()
    {
        SkeletonData skeleton;
        skeleton.joints.resize(2);
        skeleton.joints[0].parentIndex = 1;
        skeleton.joints[1].parentIndex = 0;

        const std::vector localTransforms{ Translation(1.0f, 0.0f, 0.0f), Translation(0.0f, 2.0f, 0.0f) };
        const std::vector<Mat4> originalOutput{ Translation(42.0f, 0.0f, 0.0f) };
        std::vector globalTransforms = originalOutput;
        const auto result = ComputeSkeletonGlobalTransforms(skeleton, localTransforms, globalTransforms);

        Check(result == SkeletonHierarchyResult::Cycle, "parent cycle is rejected");
        Check(globalTransforms.size() == originalOutput.size() && MatricesEqual(globalTransforms[0], originalOutput[0]), "cycle does not publish partial transforms");
    }

    void TestLocalTransformCountMismatch()
    {
        SkeletonData skeleton;
        skeleton.joints.resize(2);
        const std::vector localTransforms{ Translation(1.0f, 0.0f, 0.0f) };
        std::vector<Mat4> globalTransforms;

        Check(ComputeSkeletonGlobalTransforms(skeleton, localTransforms, globalTransforms) == SkeletonHierarchyResult::LocalTransformCountMismatch, "local transform count mismatch is rejected");
    }
} // namespace

int main()
{
    TestParentStoredAfterChild();
    TestMultipleRoots();
    TestInvalidParentIndex();
    TestParentCycle();
    TestLocalTransformCountMismatch();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " animation hierarchy test(s) failed\n";
        return 1;
    }

    std::cout << "Animation hierarchy tests passed\n";
    return 0;
}
