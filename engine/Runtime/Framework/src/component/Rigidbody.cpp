#include "ChikaEngine/Rigidbody.h"
#include "ChikaEngine/Collider.h"
#include "ChikaEngine/PhysicsSystem.h"
namespace ChikaEngine::Framework
{
    Rigidbody::~Rigidbody()
    {
        if (_physicsBodyHandle != 0)
        {
            RequestDestroy();
        }
    }

    void Rigidbody::Awake()
    {
        // if (GetOwner() && GetOwner()->transform)
        // {
        //     createDesc.transform = {
        //         .pos = GetOwner()->transform->position,
        //         .rot = GetOwner()->transform->rotation,
        //     };
        // }

        // createDesc.mass = mass;
        // createDesc.isKinematic = isKinematic;
    }

    void Rigidbody::OnEnable()
    {
        Component::OnEnable();
        Collider* col = GetOwner()->GetComponent<Collider>();
        if (col)
        {
            // 如果 Collider 已经创建了 Body，它可能是 Static 的。
            // 我们需要通知 Collider 重建 Body，以便应用 Rigidbody 的 Mass 和 MotionType
            col->RecreatePhysicsBody();
        }
        else
        {
            LOG_WARN("Rigidbody", "Rigidbody needs a Collider to function");
        }
    }

    void Rigidbody::OnDisable()
    {
        Component::OnDisable();
        // 当 Rigidbody 禁用时，物体应该变回 Static
        // 同样通知 Collider 重建
        Collider* col = GetOwner()->GetComponent<Collider>();
        if (col)
        {
            col->RecreatePhysicsBody();
        }
        _physicsBodyHandle = 0;
    }

    void Rigidbody::OnDestroy()
    {
        Component::OnDestroy();
        OnDisable();
        // if (_physicsBodyHandle != 0)
        // {
        //     RequestDestroy();
        // }
    }

    void Rigidbody::RequestCreate()
    {
        // LOG_INFO("Rigidbody", "Creating Rigidbody...");
        // // TODO: 加入对 collider 的形状描述参数
        // createDesc.ownerId = GetOwner() ? GetOwner()->GetID() : 0;
        // Physics::PhysicsSystem::Instance().EnqueueRigidbodyCreate(createDesc);
        // _isCreated = true;
    }

    void Rigidbody::RequestDestroy()
    {
        // Physics::PhysicsSystem::Instance().EnqueueRigidbodyDestroy(_physicsBodyHandle);
        // // reset statement
        // _physicsBodyHandle = 0;
        // _isCreated = false;
    }

    void Rigidbody::ApplyImpulse(const Math::Vector3& impulse)
    {
        if (_physicsBodyHandle == 0)
            return;
        Physics::PhysicsScene::Instance().ApplyImpulse(_physicsBodyHandle, impulse);
    }
    void Rigidbody::SetLinearVelocity(const ChikaEngine::Math::Vector3& vel)
    {
        if (_physicsBodyHandle == 0)
            return;
        Physics::PhysicsScene::Instance().SetLinearVelocity(_physicsBodyHandle, vel);
    }
} // namespace ChikaEngine::Framework