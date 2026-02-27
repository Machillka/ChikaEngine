#pragma once

#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/math/vector4.h"
#include <array>
// TODO:
namespace ChikaEngine::Math
{
    class Mat4
    {
      public:
        std::array<float, 16> m{};

        // 方便使用单位矩阵进行链式变化
        static constexpr Mat4 Identity()
        {
            Mat4 i{};
            i.m = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            return i;
        }
        // 提供 M(n, m) 的访问
        float& operator()(int row, int col);
        float operator()(int row, int col) const;

        Mat4& Translate(const Vector3& t);
        Mat4& Scale(const Vector3& s);
        Mat4& Rotate(float angle, const Vector3& axis);

        Mat4 Transposed() const;

        Mat4 operator*(const Mat4& rhs) const;
        Mat4 operator+(const Mat4& rhs) const;
        Vector4 operator*(const Vector4& rhs) const;

        Mat4& operator*=(const Mat4& rhs);
        Mat4& operator+=(const Mat4& rhs);

        Mat4& RotateX(float rad);
        Mat4& RotateY(float rad);
        Mat4& RotateZ(float rad);

        Mat4 Inverse() const;

        // 投影矩阵
        static Mat4 Perspective(float fov, float aspect, float zNear, float zFar);
        // 正交矩阵
        static Mat4 Orthographic(float left, float right, float bottom, float top, float zNear, float zFar);
        static Mat4 LookAt(const Vector3& position, const Vector3& target, const Vector3& up);

        Mat4 Translation(const Vector3& t);
        Mat4 Scaling(const Vector3& s);
        //  返回变化矩阵
        static Mat4 RotationX(float rad);
        static Mat4 RotationY(float rad);
        static Mat4 RotationZ(float rad);

        // TRS
        static Mat4 MakeTranslationMatrix(const Vector3& pos);
        static Mat4 MakeScaleMatrix(const Vector3& scale);
        static Mat4 MakeRotationMatrix(const Quaternion& rot);
        static Mat4 TRSMatrix(const Vector3& pos, const Quaternion& rot, const Vector3& scale);
    };
} // namespace ChikaEngine::Math