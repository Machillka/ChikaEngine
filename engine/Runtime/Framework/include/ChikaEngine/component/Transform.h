#pragma once

#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/quaternion.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"

namespace ChikaEngine::Framework
{
    MCLASS(Transform) : public Component
    {
        REFLECTION_BODY(Transform)
      public:
        Transform() = default;
        ~Transform() override = default;

        MFIELD()
        Math::Vector3 position{ 0.0f, 0.0f, 0.0f };
        MFIELD()
        Math::Quaternion rotation = Math::Quaternion::Identity();
        MFIELD()
        Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };

        // 世界空间对应操作
        MFUNCTION()
        void Translate(const Math::Vector3& delta);
        void Translate(float x, float y, float z);
        MFUNCTION()
        void Rotate(const Math::Quaternion& q);
        void Rotate(const Math::Vector3& eulerAngle);
        MFUNCTION()
        void Scale(const Math::Vector3& factor);
        void Scale(float x, float y, float z);
        void Scale(float factor);
        void LookAt(const Math::Vector3& target, const Math::Vector3& up = Math::Vector3::up);
        Math::Mat4 GetLocalMatrix() const;

        void ProcessLookDelta(float deltaX, float deltaY, bool constrainPitch = true);

        void TranslateLocal(const Math::Vector3& localDelta);
        void TranslateLocal(float x, float y, float z);

        // 在世界坐标的时候 GO对应方向
        Math::Vector3 Forward() const;
        Math::Vector3 Up() const;
        Math::Vector3 Right() const;

        Math::Mat4 GetWorldMat() const;

        void OnGizmo() const override;
    };
} // namespace ChikaEngine::Framework