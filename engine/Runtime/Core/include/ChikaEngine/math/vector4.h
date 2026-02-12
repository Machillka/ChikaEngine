#pragma once

namespace ChikaEngine::Math
{
    class Vector4
    {
      public:
        float x, y, z, w;
        constexpr Vector4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
        constexpr Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

        float Length() const;
        Vector4 Normalized() const;

        float Dot(Vector4 other) const;
        Vector4 Cross(Vector4 other) const;
        // global function
        static float Dot(const Vector4& a, const Vector4& b);
        static float Distance(const Vector4& a, const Vector4& b);
        static Vector4 Lerp(const Vector4& a, const Vector4& b, float t);
        static Vector4 Cross(const Vector4& a, const Vector4& b);

        // Operations
        friend Vector4 operator+(const Vector4& lhs, const Vector4& rhs);
        friend Vector4 operator-(const Vector4& lhs, const Vector4& rhs);
        friend Vector4 operator*(const Vector4& lhs, const Vector4& rhs);
        friend Vector4 operator*(float scalar, const Vector4& vec);
        friend Vector4 operator*(const Vector4& vec, float scalar);
        friend Vector4 operator/(const Vector4& vec, float scalar);

        // compare
        friend bool operator==(const Vector4& lhs, const Vector4& rhs);
        friend bool operator!=(const Vector4& lhs, const Vector4& rhs);

        // self updates
        Vector4& operator+=(const Vector4& rhs);
        Vector4& operator-=(const Vector4& rhs);
        Vector4& operator*=(float scalar);
        Vector4& operator/=(float scalar);
    };

} // namespace ChikaEngine::Math