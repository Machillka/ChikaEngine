#include "camera.h"

#include "math/mat4.h"
#include "math/vector3.h"

namespace ChikaEngine::Render
{

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
        return _position;
    }

    Math::Mat4 Camera::ViewMat() const
    {
        return Math::Mat4::LookAt(_position, _target, Math::Vector3::up);
    }

    Math::Mat4 Camera::ViewProjectionMat() const
    {
        return _projection * ViewMat();
    }

    Math::Mat4 Camera::ProjectionMat() const
    {
        return _projection;
    }
} // namespace ChikaEngine::Render