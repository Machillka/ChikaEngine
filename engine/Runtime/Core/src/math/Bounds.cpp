#include "ChikaEngine/math/Bounds.hpp"

#include "ChikaEngine/math/vector4.h"
#include <algorithm>
#include <cmath>

namespace ChikaEngine::Math
{
    Bounds Bounds::FromMinMax(const Vector3& minimum, const Vector3& maximum)
    {
        const Vector3 center = (minimum + maximum) * 0.5f;
        const Vector3 extents = (maximum - minimum) * 0.5f;
        return {
            .center = center,
            .extents = extents,
            .sphereRadius = extents.Length(),
            .valid = true,
        };
    }

    Bounds TransformBounds(const Bounds& localBounds, const Mat4& worldTransform)
    {
        if (!localBounds.valid)
            return {};

        const Vector4 transformedCenter = worldTransform * Vector4(localBounds.center.x, localBounds.center.y, localBounds.center.z, 1.0f);
        const Vector3 worldExtents{
            std::abs(worldTransform(0, 0)) * localBounds.extents.x + std::abs(worldTransform(0, 1)) * localBounds.extents.y + std::abs(worldTransform(0, 2)) * localBounds.extents.z,
            std::abs(worldTransform(1, 0)) * localBounds.extents.x + std::abs(worldTransform(1, 1)) * localBounds.extents.y + std::abs(worldTransform(1, 2)) * localBounds.extents.z,
            std::abs(worldTransform(2, 0)) * localBounds.extents.x + std::abs(worldTransform(2, 1)) * localBounds.extents.y + std::abs(worldTransform(2, 2)) * localBounds.extents.z,
        };

        const auto columnLength = [&](int column)
        {
            const float x = worldTransform(0, column);
            const float y = worldTransform(1, column);
            const float z = worldTransform(2, column);
            return std::sqrt(x * x + y * y + z * z);
        };
        const float maximumScale = std::max({ columnLength(0), columnLength(1), columnLength(2) });

        return {
            .center = { transformedCenter.x, transformedCenter.y, transformedCenter.z },
            .extents = worldExtents,
            .sphereRadius = localBounds.sphereRadius * maximumScale,
            .valid = true,
        };
    }

    Bounds ExpandBounds(const Bounds& bounds, float scale)
    {
        if (!bounds.valid)
            return {};
        const float conservativeScale = std::max(scale, 1.0f);
        return {
            .center = bounds.center,
            .extents = bounds.extents * conservativeScale,
            .sphereRadius = bounds.sphereRadius * conservativeScale,
            .valid = true,
        };
    }
} // namespace ChikaEngine::Math
