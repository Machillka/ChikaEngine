#pragma once
#include "math/mat4.h"
#include "math/vector3.h"
namespace ChikaEngine::Math
{
    // 四元数实现
    class Quaternion
    {
      public:
        float x, y, z, w;
        Quaternion();
        Quaternion(float x, float y, float z, float w);
        float Length() const;
        float LengthSquared() const;
        Quaternion Normalized() const;
        // 返回四元数旋转向量
        Vector3 Rotate(const Vector3& vec) const;
        static Quaternion Identity();
        // 构造四元数
        static Quaternion FromEuler(const Vector3& euler);
        static Quaternion AngleAxis(float angle, const Vector3& axis);
        Quaternion operator*(const Quaternion& rhs) const;
        static Quaternion FromToRotation(const Vector3& from, const Vector3& to);
        // 计算使得 forward 可以和 up 正交的四元数（达到物体看向forward方向的目的
        static Quaternion LookAtRotation(const Vector3& forward, const Vector3& up);
        Mat4 ToRotationMat() const;
    };

} // namespace ChikaEngine::Math