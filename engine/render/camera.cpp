#include "camera.h"

#include "math/mat4.h"
#include "math/vector3.h"

#include <cmath>


namespace ChikaEngine::Render
{
    Camera::Camera()
    {
        // 设置默认相机参数
        transform->position = Math::Vector3(0.0f, 0.0f, 2.0f);
        _target = Math::Vector3(0.0f, 0.0f, 0.0f);
        _up = Math::Vector3(0.0f, 1.0f, 0.0f);
        _projection = Math::Mat4::Perspective(3.14159f / 3.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    }
    Camera::Camera(float fovRadians, float aspect, float zNear, float zFar)
    {
        SetPerspective(fovRadians, aspect, zNear, zFar);
    }

    void Camera::SetPerspective(float fovRadians, float aspect, float zNear, float zFar)
    {
        _projection = Math::Mat4::Perspective(fovRadians, aspect, zNear, zFar);
    }

    void Camera::SetLookAt(const Math::Vector3& target)
    {
        _target = target;
    }

    void Camera::SetUp(const Math::Vector3& up)
    {
        _up = up;
    }

    const Math::Vector3& Camera::Position() const
    {
        return transform->position;
    }

    Math::Mat4 Camera::ViewMat() const
    {
        // 使用基于朝向的相机（不依赖固定 target）
        Math::Vector3 pos = transform->position;
        Math::Vector3 target = pos + _front;
        return Math::Mat4::LookAt(pos, target, _up);
    }

    Math::Mat4 Camera::ViewProjectionMat() const
    {
        return _projection * this->ViewMat();
    }

    Math::Mat4 Camera::ProjectionMat() const
    {
        return _projection;
    }

    void Camera::ProcessMouseMovement(float deltaX, float deltaY, bool constrainPitch)
    {
        const float sensitivity = 0.0025f;
        _yaw += deltaX * sensitivity;
        _pitch += -deltaY * sensitivity; // 屏幕Y向下为正 -> 反向

        if (constrainPitch)
        {
            const float limit = 1.558f; // ~89度
            if (_pitch > limit)
                _pitch = limit;
            if (_pitch < -limit)
                _pitch = -limit;
        }

        // 计算前向向量
        float cy = std::cos(_yaw);
        float sy = std::sin(_yaw);
        float cp = std::cos(_pitch);
        float sp = std::sin(_pitch);
        _front = Math::Vector3(cy * cp, sp, sy * cp).Normalized();
    }

    Math::Vector3 Camera::Front() const
    {
        return _front;
    }
} // namespace ChikaEngine::Render