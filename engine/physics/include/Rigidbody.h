#pragma once

// FIXME: 修改循环依赖问题,把 rigidbody 组件挪到 gameplay中

#include "framework/component/Component.h"
#include "include/PhysicsDescs.h"
#include "math/vector3.h"
namespace ChikaEngine::Framework
{

    class Rigidbody : public Component
    {
      public:
        Rigidbody() = default;
        ~Rigidbody() override;
        void Awake() override;
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;

        void ApplyImpulse(const Math::Vector3& impulse);
        void SetLinearVelocity(const ChikaEngine::Math::Vector3& vel);
        Physics::PhysicsBodyHandle GetBackendHandle() const
        {
            return _rigidbodyBackendHandle;
        }

        float mass = 1.0f;
        bool isKinematic = false;
        ChikaEngine::Physics::RigidbodyCreateDesc createDesc;

      private:
        Physics::PhysicsBodyHandle _rigidbodyBackendHandle;
        bool _isCreated = true;
        // 执行创建方法
        void RequestCreate();
        // 执行销毁方法
        void RequestDestroy();
    };
} // namespace ChikaEngine::Framework