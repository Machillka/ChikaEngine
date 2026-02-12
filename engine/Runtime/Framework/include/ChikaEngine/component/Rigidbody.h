#pragma once

// FIXME: 修改循环依赖问题,把 rigidbody 组件挪到 gameplay中

#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"
#include "ChikaEngine/math/vector3.h"

namespace ChikaEngine::Framework
{

    MCLASS(Rigidbody) : public Component
    {
        REFLECTION_BODY(Rigidbody)
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
            return _physicsBodyHandle;
        }

        void SetBindedHandle(Physics::PhysicsBodyHandle handle)
        {
            _physicsBodyHandle = handle;
        }

        MFIELD()
        float mass = 1.0f;
        MFIELD()
        bool isKinematic = false;

      private:
        Physics::PhysicsBodyHandle _physicsBodyHandle = 0;
        bool _isCreated = false;
        // 执行创建方法
        void RequestCreate();
        // 执行销毁方法
        void RequestDestroy();
    };
} // namespace ChikaEngine::Framework