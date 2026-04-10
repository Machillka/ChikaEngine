#include "ChikaEngine/component/Transform.h"

namespace ChikaEngine::Framework
{
    void Transform::Translate(const Math::Vector3& delta)
    {
        position += delta;
    }

    void Transform::Translate(float x, float y, float z)
    {
        Math::Vector3 delta = Math::Vector3(x, y, z);
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

    void Transform::Scale(float x, float y, float z)
    {
        scale.x *= x;
        scale.y *= y;
        scale.z *= z;
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

    void Transform::TranslateLocal(const Math::Vector3& localDelta)
    {
        // Note: Forward in local is -Z
        Math::Vector3 worldDelta = rotation.Rotate(localDelta);
        position += worldDelta;
    }

    void Transform::TranslateLocal(float x, float y, float z)
    {
        TranslateLocal(Math::Vector3(x, y, z));
    }

    Math::Mat4 Transform::GetLocalMatrix() const
    {
        return Math::Mat4::TRSMatrix(position, rotation.Normalized(), scale);
    }

    Math::Mat4 Transform::GetWorldMat() const
    {
        return GetLocalMatrix();
    }

    // TODO: 加入 Gizmo 系统
    void Transform::OnGizmo() const {}
} // namespace ChikaEngine::Framework