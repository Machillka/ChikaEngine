#pragma once

#include "math/mat4.h"
#include "math/vector3.h"

namespace ChikaEngine::Render
{
    class Camera
    {
      public:
        Camera(float fovRadians, float aspect, float zNear, float zFar);

        void SetPerspective(float fovRadians, float aspect, float zNear, float zFar);
        void SetLookAt(const Math::Vector3& pos);
        void SetUp(const Math::Vector3& up);
        const Math::Vector3& Position() const;
        Math::Mat4 ViewMat() const;
        Math::Mat4 ProjectionMat() const;
        Math::Mat4 ViewProjectionMat() const;

      private:
        Math::Vector3 _position;
        Math::Vector3 _target;
        Math::Vector3 _up;
        Math::Mat4 _projection = Math::Mat4::Identity;
    };
} // namespace ChikaEngine::Render