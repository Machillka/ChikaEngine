#pragma once

#include "math/mat4.h"
#include "math/vector3.h"

namespace ChikaEngine::Render
{
    class Camera
    {
      public:
        Camera(float fovRadians, float aspect, float zNear, float zFar);
        Camera();

        void SetPerspective(float fovRadians, float aspect, float zNear, float zFar);
        void SetLookAt(const Math::Vector3& pos);
        void SetUp(const Math::Vector3& up);
        const Math::Vector3& Position() const;
        Math::Mat4 ViewMat() const;
        Math::Mat4 ProjectionMat() const;
        Math::Mat4 ViewProjectionMat() const;
        // TODO: 实现正交等相机模式
      private:
        Math::Vector3 _position;
        Math::Vector3 _target;
        Math::Vector3 _up;
        Math::Mat4 _projection = Math::Mat4::Identity();

        // TODO: 进行封装 暴露修改接口
        float fovDegrees = 60.0f;    // 视野角度
        float aspect = 16.0f / 9.0f; // 宽高比
        float nearClip = 0.1f;
        float farClip = 1000.0f;
    };
} // namespace ChikaEngine::Render