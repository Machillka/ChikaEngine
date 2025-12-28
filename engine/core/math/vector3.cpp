#include "math/vector3.h"

#include <cmath>

namespace ChikaEngine::Math
{
    const Vector3 Vector3::zero = Vector3();
    const Vector3 Vector3::up = Vector3(0.0f, 1.0f, 0.0f);
    const Vector3 Vector3::down = Vector3(0.0f, -1.0f, 0.0f);
    const Vector3 Vector3::forward = Vector3(0.0f, 0.0f, 1.0f);
    const Vector3 Vector3::back = Vector3(0.0f, 0.0f, -1.0f);

    float Vector3::Length() const
    {
        return std::sqrt(x * x + y * y + z * z);
    }

    float Vector3::Distance(const Vector3& a, const Vector3& b)
    {
        return (a - b).Length();
    }

    Vector3 Vector3::Normalize() const
    {
        float len = Length();
        if (len > 0.0f)
            return Vector3(x / len, y / len, z / len);
        return Vector3::zero;
    }

    float Vector3::Dot(const Vector3& a, const Vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    Vector3 Vector3::Lerp(const Vector3& a, const Vector3& b, float t)
    {
        return a + (b - a) * t;
    }

    Vector3 operator+(const Vector3& lhs, const Vector3& rhs)
    {
        return Vector3(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
    }

    Vector3 operator-(const Vector3& lhs, const Vector3& rhs)
    {
        return Vector3(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
    }

    Vector3 operator*(const Vector3& vec, float scalar)
    {
        return Vector3(vec.x * scalar, vec.y * scalar, vec.z * scalar);
    }

    Vector3 operator*(float scalar, const Vector3& vec)
    {
        return Vector3(vec.x * scalar, vec.y * scalar, vec.z * scalar);
    }

    Vector3 operator/(const Vector3& vec, float scalar)
    {
        if (scalar != 0.0f)
            return Vector3(vec.x / scalar, vec.y / scalar, vec.z / scalar);
        return vec;
    }

    bool operator==(const Vector3& lhs, const Vector3& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    bool operator!=(const Vector3& lhs, const Vector3& rhs)
    {
        return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z;
    }

    Vector3& Vector3::operator+=(const Vector3& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    Vector3& Vector3::operator-=(const Vector3& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    Vector3& Vector3::operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    Vector3& Vector3::operator/=(float scalar)
    {
        if (scalar != 0.0f)
        {
            x /= scalar;
            y /= scalar;
            z /= scalar;
        }

        return *this;
    }
} // namespace ChikaEngine::Math