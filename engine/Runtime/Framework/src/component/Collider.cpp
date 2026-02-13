#include "ChikaEngine/component/Collider.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/component/Rigidbody.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.h"
#include <cstdlib>

namespace ChikaEngine::Framework
{

    void Collider::SetPhysicsScene()
    {
        _physicsScene = GetOwner()->GetScene()->GetRuntimeScenePhysics();
    }

    void Collider::OnEnable()
    {
        Component::OnEnable();
        SetPhysicsScene();

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
            _physicsScene->EnqueueRigidbodyDestroy(_physicsBodyHandle);
            // Physics::PhysicsScene::Instance().EnqueueRigidbodyDestroy(_physicsBodyHandle);
            _physicsBodyHandle = 0;
            _physicsScene = nullptr;
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
            // TODO: 实现 Scene 中得到 Physics 然后进行一个改的修
            // 因为覆盖 所以允许延迟删除
            _physicsScene->EnqueueRigidbodyDestroy(_physicsBodyHandle);
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
        desc.layer = static_cast<Physics::PhysicsLayerID>(layer);

        // Physics::PhysicsScene::Instance().SetLayerMask(desc.layerMask, collisionMask);
        _physicsScene->SetLayerCollisionMask(desc.layer, collisionMask);
        // _physicsBodyHandle = Physics::PhysicsScene::Instance().CreateBodyImmediate(desc);
        _physicsScene->CreateBodyImmediate(desc);
        LOG_DEBUG("Physics", "Set {}", _physicsBodyHandle);
        if (rb)
        {
            rb->SetBindedHandle(_physicsBodyHandle);
        }
    }

} // namespace ChikaEngine::Framework