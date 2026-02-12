#pragma once

#include "ChikaEngine/reflection/ReflectionMacros.h"

namespace ChikaEngine::Math
{

    MCLASS(Vector3)
    {
        REFLECTION_BODY(Vector3)
      public:
        MFIELD()
        float x;
        MFIELD()
        float y;
        MFIELD()
        float z;
        constexpr Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
        constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

        float Length() const;
        Vector3 Normalized() const;

        float Dot(Vector3 other) const;
        Vector3 Cross(Vector3 other) const;
        // global function
        static float Dot(const Vector3& a, const Vector3& b);
        static float Distance(const Vector3& a, const Vector3& b);
        static Vector3 Lerp(const Vector3& a, const Vector3& b, float t);
        static Vector3 Cross(const Vector3& a, const Vector3& b);

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
        static const Vector3 left;
        static const Vector3 right;
    };

} // namespace ChikaEngine::Math