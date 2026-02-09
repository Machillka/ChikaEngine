#pragma once
#include "Component.h"
#include "math/ChikaMath.h"
#include "math/mat4.h"
#include "reflection/ReflectionMacros.h"
namespace ChikaEngine::Framework
{
    // TODO: 加上父子层级关系
    MCLASS(Transform) : public Component
    {
      public:
        Transform() = default;
        ~Transform() override = default;

        // 基于 世界坐标 的变换
        MFIELD()
        Math::Vector3 position{0.0f, 0.0f, 0.0f};
        Math::Quaternion rotation = Math::Quaternion::Identity();
        Math::Vector3 scale{1.0f, 1.0f, 1.0f};

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
        // Camera-like helpers: process mouse look deltas (in pixels) and local-space translation
        void ProcessLookDelta(float deltaX, float deltaY, bool constrainPitch = true);

        // Translate in local space (right, up, forward) where forward is -Z in local coordinates
        void TranslateLocal(const Math::Vector3& localDelta);
        void TranslateLocal(float x, float y, float z);

        // 在世界坐标的时候 GO对应方向
        Math::Vector3 Forward() const;
        Math::Vector3 Up() const;
        Math::Vector3 Right() const;
        // 得到世界坐标的矩阵
        Math::Mat4 GetWorldMat() const;

      private:
        // store yaw/pitch for look processing (radians)
        float _yaw = -Math::PI / 2.0f; // match previous camera default
        float _pitch = 0.0f;
    };
} // namespace ChikaEngine::Framework