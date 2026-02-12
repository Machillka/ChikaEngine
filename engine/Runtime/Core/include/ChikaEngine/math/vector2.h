#pragma once

namespace ChikaEngine::Math
{
    class Vector2
    {
      public:
        float x, y;
        constexpr Vector2() : x(0.0f), y(0.0f) {}
        constexpr Vector2(float x, float y) : x(x), y(y) {}

        float Length() const;
        Vector2 Normalize() const;

        // global function
        static float Dot(const Vector2& a, const Vector2& b);
        static float Distance(const Vector2& a, const Vector2& b);
        static Vector2 Lerp(const Vector2& a, const Vector2& b, float t);

        // Operations
        friend Vector2 operator+(const Vector2& lhs, const Vector2& rhs);
        friend Vector2 operator-(const Vector2& lhs, const Vector2& rhs);
        friend Vector2 operator*(const Vector2& lhs, const Vector2& rhs);
        friend Vector2 operator*(float scalar, const Vector2& vec);
        friend Vector2 operator*(const Vector2& vec, float scalar);
        friend Vector2 operator/(const Vector2& vec, float scalar);

        // compare
        friend bool operator==(const Vector2& lhs, const Vector2& rhs);
        friend bool operator!=(const Vector2& lhs, const Vector2& rhs);

        // self updates
        Vector2& operator+=(const Vector2& rhs);
        Vector2& operator-=(const Vector2& rhs);
        Vector2& operator*=(float scalar);
        Vector2& operator/=(float scalar);

        // constance
        static const Vector2 zero;
        static const Vector2 up;
        static const Vector2 down;
        static const Vector2 forward;
        static const Vector2 back;
    };

} // namespace ChikaEngine::Math