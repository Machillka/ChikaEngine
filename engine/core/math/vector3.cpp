#include "math/vector3.h"

namespace ChikaEngine::Math
{
    float Vector3::Length() const
    {
        return std::sqrt(x * x + y * y + z * z);
    }

    float Vector3::Distance(const Vector3 &a, const Vector3 &b)
    {
        return (a - b).Length();
    }
} // namespace ChikaEngine::Math