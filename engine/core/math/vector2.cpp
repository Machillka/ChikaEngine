#include "math/vector2.h"

#include "debug/assert.h"

#include <cmath>

namespace ChikaEngine::Math
{
    const Vector2 Vector2::zero = Vector2();
    const Vector2 Vector2::up = Vector2(1.0f, 0.0f);
    const Vector2 Vector2::down = Vector2(-1.0f, 0.0f);
    const Vector2 Vector2::forward = Vector2(0.0f, 1.0f);
    const Vector2 Vector2::back = Vector2(0.0f, -1.0f);

    float Vector2::Length() const
    {
        return std::sqrt(x * x + y * y);
    }

    float Vector2::Distance(const Vector2& a, const Vector2& b)
    {
        return (a - b).Length();
    }

    Vector2 Vector2::Normalize() const
    {
        float len = Length();
        CHIKA_ASSERT(len > 0.0f, "Vector length less than zero");
        return Vector2(x / len, y / len);
    }

    float Vector2::Dot(const Vector2& a, const Vector2& b)
    {
        return a.x * b.x + a.y * b.y;
    }

    Vector2 Vector2::Lerp(const Vector2& a, const Vector2& b, float t)
    {
        return a + (b - a) * t;
    }

    Vector2 operator+(const Vector2& lhs, const Vector2& rhs)
    {
        return Vector2(lhs.x + rhs.x, lhs.y + rhs.y);
    }

    Vector2 operator-(const Vector2& lhs, const Vector2& rhs)
    {
        return Vector2(lhs.x - rhs.x, lhs.y - rhs.y);
    }

    Vector2 operator*(const Vector2& vec, float scalar)
    {
        return Vector2(vec.x * scalar, vec.y * scalar);
    }

    Vector2 operator*(float scalar, const Vector2& vec)
    {
        return Vector2(vec.x * scalar, vec.y * scalar);
    }

    Vector2 operator/(const Vector2& vec, float scalar)
    {
        CHIKA_ASSERT(scalar != 0.0f, "Division by zero error");
        return Vector2(vec.x / scalar, vec.y / scalar);
    }

    bool operator==(const Vector2& lhs, const Vector2& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    }

    bool operator!=(const Vector2& lhs, const Vector2& rhs)
    {
        return lhs.x != rhs.x || lhs.y != rhs.y;
    }

    Vector2& Vector2::operator+=(const Vector2& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    Vector2& Vector2::operator-=(const Vector2& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    Vector2& Vector2::operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    Vector2& Vector2::operator/=(float scalar)
    {
        CHIKA_ASSERT(scalar != 0.0f, "Division by zero error");

        x /= scalar;
        y /= scalar;

        return *this;
    }
} // namespace ChikaEngine::Math