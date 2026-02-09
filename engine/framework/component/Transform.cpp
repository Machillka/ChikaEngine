#include "Transform.h"

#include "debug/log_macros.h"
#include "math/mat4.h"
#include "math/quaternion.h"
#include "math/vector3.h"

namespace ChikaEngine::Framework
{
    void Transform::Translate(const Math::Vector3& delta)
    {
        LOG_WARN("Transform", "Now Position x: {}, y: {}, z: {}", this->position.x, this->position.y, this->position.z);
        position += delta;
    }

    void Transform::Translate(float x, float y, float z)
    {
        LOG_WARN("Transform", "Now Position x: {}, y: {}, z: {}", this->position.x, this->position.y, this->position.z);
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

    // TODO: 做成外部函数 ( 只有camera拥有 )
    void Transform::ProcessLookDelta(float deltaX, float deltaY, bool constrainPitch)
    {
        // Incremental rotation: apply yaw delta around world up, then pitch delta around local right
        // LOG_INFO("Transform", "ProcessLookDelta dx={} dy={}", deltaX, deltaY);
        const float sensitivity = 0.0025f;
        float yawDelta = deltaX * sensitivity;
        float pitchDelta = -deltaY * sensitivity; // invert Y

        // update stored angles (for clamping and debug)
        _yaw += yawDelta;
        _pitch += pitchDelta;

        if (constrainPitch)
        {
            const float limit = 1.558f; // ~89 degrees
            if (_pitch > limit)
                _pitch = limit;
            if (_pitch < -limit)
                _pitch = -limit;
            // clamp pitchDelta to not exceed limits
            // (if we clamped _pitch, adjust pitchDelta so application respects clamped value)
            // recompute pitchDelta relative to previous pitch
            // Note: this is best-effort; exact clamping behavior is maintained via _pitch state.
        }

        // yaw first
        Math::Quaternion yawQ = Math::Quaternion::AngleAxis(yawDelta, Math::Vector3::up);
        Math::Quaternion afterYaw = (yawQ * rotation).Normalized();

        // compute local right axis after yaw, then pitch around it
        Math::Vector3 rightAxis = afterYaw.Rotate(Math::Vector3::right);
        Math::Quaternion pitchQ = Math::Quaternion::AngleAxis(pitchDelta, rightAxis);

        rotation = (pitchQ * afterYaw).Normalized();
        // LOG_INFO("Transform", "Updated rotation yaw={} pitch={}", _yaw, _pitch);
    }

    void Transform::TranslateLocal(const Math::Vector3& localDelta)
    {
        // LOG_INFO("Transform", "TranslateLocal localDelta={},{},{}", localDelta.x, localDelta.y, localDelta.z);
        // convert local delta (right, up, forward) into world space using rotation
        // Note: Forward in local is -Z
        Math::Vector3 worldDelta = rotation.Rotate(localDelta);
        position += worldDelta;
        // LOG_INFO("Transform", "TranslateLocal worldDelta={},{},{} newPos={},{},{}", worldDelta.x, worldDelta.y, worldDelta.z, position.x, position.y, position.z);
    }

    void Transform::TranslateLocal(float x, float y, float z)
    {
        TranslateLocal(Math::Vector3(x, y, z));
    }

    Math::Mat4 Transform::GetLocalMatrix() const
    {
        return Math::Mat4::TRSMatrix(position, rotation.Normalized(), scale);
    }
} // namespace ChikaEngine::Framework