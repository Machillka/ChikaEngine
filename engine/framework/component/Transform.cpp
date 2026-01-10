#include "Transform.h"

#include "math/quaternion.h"
#include "math/vector3.h"

namespace ChikaEngine::Framework
{
    void Transform::Translate(const Math::Vector3& delta)
    {
        position += delta;
    }

    void Transform::Rotate(const Math::Quaternion& q)
    {
        rotation = (q * rotation).Normalized();
    }

    void Transform::Rotate(const Math::Vector3& eulerAngle)
    {
        rotation = Math::Quaternion::FromEuler(eulerAngle) * rotation;
        rotation = rotation.Normalized();
    }

    void Transform::Scale(const Math::Vector3& factor)
    {
        scale.x *= factor.x;
        scale.y *= factor.y;
        scale.z *= factor.z;
    }

    void Transform::Scale(float factor)
    {
        scale = scale * factor;
    }

    void Transform::LookAt(const Math::Vector3& target, const Math::Vector3& up)
    {
        // 检查当前位置和目标位置是否相同
        Math::Vector3 forwardVec = target - position;
        if (forwardVec.Length() < 1e-6f)
        {
            return; // 无法创建有效的朝向
        }
        Math::Vector3 forward = forwardVec.Normalized();
        rotation = Math::Quaternion::LookAtRotation(forward, up);
    }

    Math::Vector3 Transform::Forward() const
    {
        // 把“默认前方(-Z)”通过当前旋转旋转到世界空间，得到物体真正的前方。
        return rotation.Rotate(Math::Vector3::back);
    }

    Math::Vector3 Transform::Right() const
    {
        return rotation.Rotate(Math::Vector3::right);
    }

    Math::Vector3 Transform::Up() const
    {
        return rotation.Rotate(Math::Vector3::up);
    }
} // namespace ChikaEngine::Framework