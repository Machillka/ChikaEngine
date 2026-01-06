#include "quaternion.h"

#include "math/vector3.h"

#include <cmath>
#include <stddef.h>

namespace ChikaEngine::Math
{
    Quaternion::Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    Quaternion::Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Quaternion Quaternion::Identity()
    {
        return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }
    Quaternion Quaternion::FromEuler(const Vector3& euler)
    {
        float cx = std::cos(euler.x * 0.5f);
        float sx = std::sin(euler.x * 0.5f);
        float cy = std::cos(euler.y * 0.5f);
        float sy = std::sin(euler.y * 0.5f);
        float cz = std::cos(euler.z * 0.5f);
        float sz = std::sin(euler.z * 0.5f);
        Quaternion q;
        q.w = cx * cy * cz + sx * sy * sz;
        q.x = sx * cy * cz - cx * sy * sz;
        q.y = cx * sy * cz + sx * cy * sz;
        q.z = cx * cy * sz - sx * sy * cz;
        return q;
    }
    Vector3 Quaternion::Rotate(const Vector3& vec) const
    {
        Quaternion vecQuat(vec.x, vec.y, vec.z, 0.0f);
        // 共轭四元数（假设四元数已单位化）
        Quaternion conjugate(-x, -y, -z, w);
        // 计算旋转: q * v * q^(-1)
        Quaternion resQuat = (*this) * vecQuat * conjugate;

        return Vector3(resQuat.x, resQuat.y, resQuat.z);
    }
    Quaternion Quaternion::AngleAxis(float angle, const Vector3& axis)
    {
        float half = angle * 0.5f;
        float s = std::sin(half);
        return Quaternion(axis.x * s, axis.y * s, axis.z * s, std::cos(half));
    }
    Quaternion Quaternion::operator*(const Quaternion& rhs) const
    {
        return Quaternion(w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
                          w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
                          w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
                          w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z);
    }
    Quaternion Quaternion::Normalized() const
    {
        float len = std::sqrt(x * x + y * y + z * z + w * w);
        return Quaternion(x / len, y / len, z / len, w / len);
    }

    // Copy From Copilot
    Quaternion Quaternion::FromToRotation(const Vector3& from, const Vector3& to)
    {
        // 检查输入向量长度
        float fromLen = from.Length();
        float toLen = to.Length();
        if (fromLen < 1e-6f || toLen < 1e-6f)
        {
            return Quaternion::Identity();
        }

        Vector3 f = from.Normalized();
        Vector3 t = to.Normalized();
        float cosTheta = f.Dot(t);

        // 相同方向，无需旋转
        if (cosTheta > 0.9999f)
        {
            return Quaternion::Identity();
        }

        // 180° 特殊情况
        if (cosTheta < -0.9999f)
        {
            Vector3 axis = Vector3(0, 1, 0).Cross(f);
            if (axis.Length() < 1e-6f)
                axis = Vector3(1, 0, 0).Cross(f);
            if (axis.Length() < 1e-6f) // 防止向量仍为零
                axis = Vector3(0, 0, 1).Cross(f);
            return AngleAxis(3.1415926f, axis.Normalized());
        }

        // 普通情况
        Vector3 axis = f.Cross(t);
        float axisLen = axis.Length();
        if (axisLen < 1e-6f) // 防止零向量
        {
            return Quaternion::Identity();
        }

        float s = std::sqrt((1 + cosTheta) * 2);
        float invs = 1.0f / s;
        return Quaternion(axis.x * invs, axis.y * invs, axis.z * invs, s * 0.5f).Normalized();
    }

    Quaternion Quaternion::LookAtRotation(const Vector3& forward, const Vector3& up)
    {
        Vector3 f = forward.Normalized();
        Quaternion r = FromToRotation(Vector3::back, f);
        Vector3 rotatedUp = r.Rotate(Vector3::up);
        Quaternion rotUp = FromToRotation(rotatedUp, up.Normalized());
        return (rotUp * r).Normalized();
    }
    Mat4 Quaternion::ToRotationMat() const
    {
        Mat4 m = Mat4::Identity();
        float xx = x * x, yy = y * y, zz = z * z;
        float xy = x * y, xz = x * z, yz = y * z;
        float wx = w * x, wy = w * y, wz = w * z;
        m(0, 0) = 1 - 2 * (yy + zz);
        m(0, 1) = 2 * (xy - wz);
        m(0, 2) = 2 * (xz + wy);
        m(1, 0) = 2 * (xy + wz);
        m(1, 1) = 1 - 2 * (xx + zz);
        m(1, 2) = 2 * (yz - wx);
        m(2, 0) = 2 * (xz - wy);
        m(2, 1) = 2 * (yz + wx);
        m(2, 2) = 1 - 2 * (xx + yy);
        return m;
    }
} // namespace ChikaEngine::Math