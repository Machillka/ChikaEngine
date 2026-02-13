
#include "ChikaEngine/gameobject/Camera.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/render_device.h"

namespace ChikaEngine::Framework
{
    Camera::Camera(std::string name) : GameObject(name)
    {
        // 设置默认相机参数
        transform->position = Math::Vector3(0.0f, 0.0f, 2.0f);
        _target = Math::Vector3(0.0f, 0.0f, 0.0f);
        // _up = Math::Vector3(0.0f, 1.0f, 0.0f);
        // TODO: 提供修改 up 的重新计算的方法
        _projection = Math::Mat4::Perspective(3.14159f / 3.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    }

    void Camera::SetPerspective(float fovRadians, float aspect, float zNear, float zFar)
    {
        _projection = Math::Mat4::Perspective(fovRadians, aspect, zNear, zFar);
    }

    void Camera::SetLookAt(const Math::Vector3& target)
    {
        _target = target;
    }

    const Math::Vector3& Camera::Position() const
    {
        return transform->position;
    }

    Math::Mat4 Camera::ViewMat() const
    {
        Math::Vector3 pos = transform->position;
        Math::Vector3 front = transform->Forward();
        Math::Vector3 target = pos + front;
        Math::Vector3 up = transform->Up();
        return Math::Mat4::LookAt(pos, target, up);
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
        // Delegate orientation changes to Transform
        transform->ProcessLookDelta(deltaX, deltaY, constrainPitch);
    }
    Math::Vector3 Camera::Front() const
    {
        return transform->Forward();
    }

    Render::CameraData Camera::ToRenderData() const
    {
        return Render::CameraData{.position = Position(), .projectionMatrix = ProjectionMat(), .viewMatrix = ViewMat()};
    }
} // namespace ChikaEngine::Framework