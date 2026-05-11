#pragma once

#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/math/quaternion.h"

namespace ChikaEngine::Render
{
    enum class CameraType
    {
        Perspective,
        Orthographic
    };

    class Camera
    {
      public:
        Camera();
        ~Camera() = default;

        void SetPerspective(float fov, float aspect, float near, float far);
        void SetPosition(const Math::Vector3& pos);
        void SetRotation(const Math::Vector3& eulerDegrees);
        void SetLookAt(const Math::Vector3& target);

        void Rotate(const Math::Vector3& deltaEuler);

        const Math::Mat4& GetViewMatrix();
        const Math::Mat4& GetProjectionMatrix();
        const Math::Mat4& GetViewProjectionMatrix();

        const Math::Vector3& GetRotation() const
        {
            return m_rotation;
        }
        const Math::Vector3 GetPosition() const
        {
            return m_position;
        }
        Math::Vector3 GetForward() const;

        const float GetFOV() const
        {
            return m_fov;
        }
        void SetFOV(float fov)
        {
            m_fov = fov;
            m_isDirty = true;
        }
        void SetAspectRatio(float aspect)
        {
            m_aspect = aspect;
            m_isDirty = true;
        }

      private:
        void UpdateMatrices();

      private:
        CameraType m_type = CameraType::Perspective;
        Math::Vector3 m_position{ 0.0f, 0.0f, 5.0f };
        Math::Vector3 m_rotation{ 0.0f, 0.0f, 0.0f };

        float m_fov = 45.0f;
        float m_aspect = 1.77f;
        float m_near = 0.1f;
        float m_far = 1000.0f;

        Math::Mat4 m_viewMatrix;
        Math::Mat4 m_projMatrix;
        Math::Mat4 m_vpMatrix;

        bool m_isDirty = true; // 实现懒惰更新, 在访问的时候只在属性 update 的时候重新计算
    };
} // namespace ChikaEngine::Render