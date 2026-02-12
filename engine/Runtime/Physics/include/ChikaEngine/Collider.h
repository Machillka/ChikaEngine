#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/layer/layer.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/reflection/ReflectionMacros.h"
namespace ChikaEngine::Framework
{
    // TODO: 把 Collider 和 Rigidbody 解耦, 并且使得 Collider 可以独立存在成为Trigger或者其他

    MCLASS(Collider) : public Component
    {
        REFLECTION_BODY(Collider)
      public:
        Collider() = default;
        ~Collider() override = default;
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;

        // 形状参数
        Physics::RigidbodyShapes shape = Physics::RigidbodyShapes::Box;
        MFIELD()
        Math::Vector3 center = {0, 0, 0};
        MFIELD()
        Math::Vector3 size = {1, 1, 1}; // Box
        MFIELD()
        float radius = 0.5f; // Sphere
        MFIELD()
        bool isTrigger = false;

        // TODO: 单独创建物理材质 Physics Material
        MFIELD()
        float friction = 0.5f;
        MFIELD()
        float restitution = 0.0f;

        // 运行时重构 body
        MFUNCTION()
        void RecreatePhysicsBody();
        // Layer Settings
        GameObjectLayer layer = GameObjectLayer::Default;
        LayerMask collisionMask = MakeMask({GameObjectLayer::Default});

      private:
        Physics::PhysicsBodyHandle _physicsBodyHandle = 0;
    };
} // namespace ChikaEngine::Framework