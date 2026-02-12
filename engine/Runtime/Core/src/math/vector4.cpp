#include "ChikaEngine/math/vector4.h"
#include <ChikaEngine/debug/assert.h>

#include <cmath>

namespace ChikaEngine::Math
{
    float Vector4::Length() const
    {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }

    float Vector4::Distance(const Vector4& a, const Vector4& b)
    {
        return (a - b).Length();
    }

    Vector4 Vector4::Normalized() const
    {
        float len = Length();
        CHIKA_ASSERT(len > 0.0f, "Vector length less than zero");
        return Vector4(x / len, y / len, z / len, w / len);
    }

    float Vector4::Dot(Vector4 other) const
    {
        return Vector4::Dot(*this, other);
    }

    float Vector4::Dot(const Vector4& a, const Vector4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    Vector4 Vector4::Lerp(const Vector4& a, const Vector4& b, float t)
    {
        return a + (b - a) * t;
    }

    Vector4 operator+(const Vector4& lhs, const Vector4& rhs)
    {
        return Vector4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
    }

    Vector4 operator-(const Vector4& lhs, const Vector4& rhs)
    {
        return Vector4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
    }

    Vector4 operator*(const Vector4& vec, float scalar)
    {
        return Vector4(vec.x * scalar, vec.y * scalar, vec.z * scalar, vec.w * scalar);
    }

    Vector4 operator*(float scalar, const Vector4& vec)
    {
        return Vector4(vec.x * scalar, vec.y * scalar, vec.z * scalar, vec.w * scalar);
    }

    Vector4 operator/(const Vector4& vec, float scalar)
    {
        CHIKA_ASSERT(scalar != 0.0f, "Division by zero error");
        return Vector4(vec.x / scalar, vec.y / scalar, vec.z / scalar, vec.w / scalar);
    }

    bool operator==(const Vector4& lhs, const Vector4& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
    }

    bool operator!=(const Vector4& lhs, const Vector4& rhs)
    {
        return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z || lhs.w != rhs.w;
    }

    Vector4& Vector4::operator+=(const Vector4& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
        return *this;
    }

    Vector4& Vector4::operator-=(const Vector4& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
        return *this;
    }

    Vector4& Vector4::operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }

    Vector4& Vector4::operator/=(float scalar)
    {
        CHIKA_ASSERT(scalar != 0.0f, "Division by zero error");

        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;

        return *this;
    }
} // namespace ChikaEngine::Math