#pragma once

namespace ChikaEngine::Math
{
    class Vector3
    {
      public:
        float x, y, z;
        constexpr Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
        constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

        float Length() const;
        Vector3 Normalize() const;

        // global function
        static float Dot(const Vector3& a, const Vector3& b);
        static float Distance(const Vector3& a, const Vector3& b);
        static Vector3 Lerp(const Vector3& a, const Vector3& b, float t);

        // Operations
        friend Vector3 operator+(const Vector3& lhs, const Vector3& rhs);
        friend Vector3 operator-(const Vector3& lhs, const Vector3& rhs);
        friend Vector3 operator*(const Vector3& lhs, const Vector3& rhs);
        friend Vector3 operator*(float scalar, const Vector3& vec);
        friend Vector3 operator*(const Vector3& vec, float scalar);
        friend Vector3 operator/(const Vector3& vec, float scalar);

        // compare
        friend bool operator==(const Vector3& lhs, const Vector3& rhs);
        friend bool operator!=(const Vector3& lhs, const Vector3& rhs);

        // self updates
        Vector3& operator+=(const Vector3& rhs);
        Vector3& operator-=(const Vector3& rhs);
        Vector3& operator*=(float scalar);
        Vector3& operator/=(float scalar);

        // constance
        static const Vector3 zero;
        static const Vector3 up;
        static const Vector3 down;
        static const Vector3 forward;
        static const Vector3 back;
    };

} // namespace ChikaEngine::Math