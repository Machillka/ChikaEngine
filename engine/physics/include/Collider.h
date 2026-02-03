#pragma once

#include "framework/component/Component.h"
#include "framework/layer/layer.h"
#include "math/vector3.h"
namespace ChikaEngine::Framework
{
    enum class ColliderShapeTypes
    {
        Box,
        Sphere
    };
    // TODO: 把 Collider 和 Rigidbody 解耦, 并且使得 Collider 可以独立存在成为Trigger或者其他
    class Collider : public Component
    {
      public:
        Collider() = default;
        ~Collider() override = default;
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;

        // 形状参数
        ColliderShapeTypes shape = ColliderShapeTypes::Box;
        Math::Vector3 halfExtents{0.5f, 0.5f, 0.5f};
        float radius = 0.5f;

        // Layer Settings
        GameObjectLayer layer = GameObjectLayer::Default;
        LayerMask collisionMask = MakeMask({GameObjectLayer::Default});

      private:
        bool _isCreated;
    };
} // namespace ChikaEngine::Framework