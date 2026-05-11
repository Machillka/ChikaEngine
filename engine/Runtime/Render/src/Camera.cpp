
#include "ChikaEngine/Camera.hpp"
#include <algorithm>
#include <cmath>
#include "ChikaEngine/math/ChikaMath.h"

namespace ChikaEngine::Render
{
    Camera::Camera()
    {
        // 初始化时立即计算一次矩阵
        UpdateMatrices();
    }

    void Camera::SetPerspective(float fov, float aspect, float nearClip, float farClip)
    {
        m_fov = fov;
        m_aspect = aspect;
        m_near = nearClip;
        m_far = farClip;
        m_isDirty = true; // 标记数据已脏，需要重新计算
    }

    void Camera::SetPosition(const Math::Vector3& pos)
    {
        m_position = pos;
        m_isDirty = true;
    }

    void Camera::SetRotation(const Math::Vector3& eulerDegrees)
    {
        m_rotation = eulerDegrees;

        // 限制 Pitch 范围，防止万向节死锁或镜头翻转
        m_rotation.x = std::clamp(m_rotation.x, -89.0f, 89.0f);

        m_isDirty = true;
    }

    void Camera::SetLookAt(const Math::Vector3& target)
    {
        Math::Vector3 forward = (target - m_position).Normalized();

        float pitch = std::asin(forward.y);
        float yaw = std::atan2(-forward.x, -forward.z);

        m_rotation.x = pitch * Math::RAD2DEG;
        m_rotation.y = yaw * Math::RAD2DEG;
        m_rotation.z = 0.0f;

        m_isDirty = true;
    }

    Math::Vector3 Camera::GetForward() const
    {
        Math::Vector3 radRot = m_rotation * Math::DEG2RAD;

        Math::Quaternion q = Math::Quaternion::FromEuler(radRot);

        return q.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f)).Normalized();
    }

    void Camera::Rotate(const Math::Vector3& deltaEuler)
    {
        m_rotation += deltaEuler;

        // 限制 Pitch
        m_rotation.x = std::clamp(m_rotation.x, -89.0f, 89.0f);

        m_isDirty = true;
    }

    const Math::Mat4& Camera::GetViewMatrix()
    {
        if (m_isDirty)
            UpdateMatrices();
        return m_viewMatrix;
    }

    const Math::Mat4& Camera::GetProjectionMatrix()
    {
        if (m_isDirty)
            UpdateMatrices();
        return m_projMatrix;
    }

    const Math::Mat4& Camera::GetViewProjectionMatrix()
    {
        if (m_isDirty)
            UpdateMatrices();
        return m_vpMatrix;
    }

    void Camera::UpdateMatrices()
    {
        if (!m_isDirty)
            return;

        m_projMatrix = Math::Mat4::Perspective(m_fov * Math::DEG2RAD, m_aspect, m_near, m_far);
        m_projMatrix(1, 1) *= -1;

        Math::Vector3 radRot(m_rotation.x * Math::DEG2RAD, m_rotation.y * Math::DEG2RAD, m_rotation.z * Math::DEG2RAD);
        Math::Quaternion rotation = Math::Quaternion::FromEuler(radRot);

        Math::Vector3 forward = rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
        Math::Vector3 up = rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));

        m_viewMatrix = Math::Mat4::LookAt(m_position, m_position + forward, up);

        m_vpMatrix = m_projMatrix * m_viewMatrix;

        m_isDirty = false;
    }

} // namespace ChikaEngine::Render