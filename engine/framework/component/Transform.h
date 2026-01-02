#pragma once
#include "Component.h"
#include "math/ChikaMath.h"
namespace ChikaEngine::Framework
{
    // TODO: 加上父子层级关系
    class Transform : public Component
    {
      public:
        Transform() = default;
        ~Transform() override = default;

        // 基于 世界坐标 的变换
        Math::Vector3 position{0.0f, 0.0f, 0.0f};
        Math::Quaternion rotation = Math::Quaternion::Identity();
        Math::Vector3 scale{1.0f, 1.0f, 1.0f};

        // 世界空间对应操作
        void Translate(const Math::Vector3& delta);
        void Rotate(const Math::Quaternion& q);
        void Rotate(const Math::Vector3& eulerAngle);
        void Scale(const Math::Vector3& factor);
        void Scale(float factor);
        void LookAt(const Math::Vector3& target, const Math::Vector3& up = Math::Vector3::up);

        // 在世界坐标的时候 GO对应方向
        Math::Vector3 Forward() const;
        Math::Vector3 Up() const;
        Math::Vector3 Right() const;
        // 得到世界坐标的矩阵
        Math::Mat4 GetWorldMat() const;
    };
} // namespace ChikaEngine::Framework