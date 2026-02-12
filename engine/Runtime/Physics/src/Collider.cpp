#include "ChikaEngine/Collider.h"
#include "ChikaEngine/PhysicsSystem.h"
#include "ChikaEngine/Rigidbody.h"
#include <cstdint>
#include <cstdlib>

namespace ChikaEngine::Framework
{
    void Collider::OnEnable()
    {
        Component::OnEnable();

        if (_physicsBodyHandle == 0)
        {
            RecreatePhysicsBody();
        }
    }
    void Collider::OnDisable()
    {
        Component::OnDisable();
        if (_physicsBodyHandle != 0)
        {
            Physics::PhysicsSystem::Instance().EnqueueRigidbodyDestroy(_physicsBodyHandle);
            _physicsBodyHandle = 0;
        }
    }
    void Collider::OnDestroy()
    {
        Component::OnDestroy();
        OnDisable();
    }

    void Collider::RecreatePhysicsBody()
    {
        if (_physicsBodyHandle != 0)
        {
            // 因为覆盖 所以允许延迟删除
            Physics::PhysicsSystem::Instance().EnqueueRigidbodyDestroy(_physicsBodyHandle);
            _physicsBodyHandle = 0;
        }

        Physics::PhysicsBodyCreateDesc desc;
        desc.ownerId = GetOwner()->GetID();

        if (auto* transform = GetOwner()->transform)
        {
            desc.position = transform->position;
            desc.rotation = transform->rotation;
        }

        desc.shapeDesc.type = shape;
        desc.shapeDesc.center = center;
        desc.shapeDesc.halfExtents = size * 0.5f; // Size -> HalfExtents
        desc.shapeDesc.radius = radius;
        desc.isTrigger = isTrigger;

        // 尝试查询 rb
        Rigidbody* rb = GetOwner()->GetComponent<Rigidbody>();
        if (rb && rb->IsEnabled())
        {
            // 如果有 Rigidbody，使用它的动力学参数
            desc.motionType = rb->isKinematic ? Physics::MotionType::Kinematic : Physics::MotionType::Dynamic;
            desc.mass = rb->mass;
            LOG_INFO("Collider", "Found rigidbody:{}", rb->mass);
        }
        else
        {
            // 如果没有 Rigidbody，默认为静态物体
            desc.motionType = Physics::MotionType::Static;
        }

        desc.friction = friction;
        desc.restitution = restitution;
        desc.layerMask = static_cast<uint32_t>(layer);

        Physics::PhysicsSystem::Instance().SetLayerMask(desc.layerMask, collisionMask);

        _physicsBodyHandle = Physics::PhysicsSystem::Instance().CreateBodyImmediate(desc);
        LOG_DEBUG("Physics", "Set {}", _physicsBodyHandle);
        if (rb)
        {
            rb->SetBindedHandle(_physicsBodyHandle);
        }
    }

} // namespace ChikaEngine::Framework