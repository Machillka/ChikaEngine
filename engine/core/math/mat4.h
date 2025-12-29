#pragma once

#include "debug/assert.h"
#include "vector3.h"

#include <array>

namespace ChikaEngine::Math
{
    class Mat4
    {
      public:
        std::array<float, 16> m{};

        static const Mat4 Identity;

        // 提供 M(n, m) 的访问
        float& operator()(int row, int col);
        float operator()(int row, int col) const;

        Mat4& Translate(const Vector3& t);
        Mat4& Scale(const Vector3& s);
        Mat4& Rotate(float angle, const Vector3& axis);

        Mat4 Transpose() const;

        Mat4 operator*(const Mat4& rhs) const;
        Mat4 operator+(const Mat4& rhs) const;

        Mat4& operator*=(const Mat4& rhs);
        Mat4& operator+=(const Mat4& rhs);

        static Mat4 Perspective(float fov, float aspect, float zNear, float zFar);
        static Mat4 Orthographic(float left, float right, float bottom, float top, float near, float far);
        static Mat4 LookAt(const Vector3& position, const Vector3& target, const Vector3& up);
        static Mat4 RotateX(float angle);
        static Mat4 RotateY(float angle);
        static Mat4 RotateZ(float angle);
    };
} // namespace ChikaEngine::Math