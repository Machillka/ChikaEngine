#include "physics/include/Rigidbody.h"
#include "framework/component/Component.h"
#include "include/PhysicsDescs.h"
#include "include/PhysicsSystem.h"

namespace ChikaEngine::Framework
{
    void Rigidbody::Awake()
    {
        if (GetOwner() && GetOwner()->transform)
        {
            createDesc.transform = {
                .pos = GetOwner()->transform->position,
                .rot = GetOwner()->transform->rotation,
            };
        }

        createDesc.mass = mass;
        createDesc.isKinematic = isKinematic;
    }

    void Rigidbody::OnEnable()
    {
        Component::OnEnable();
        // 在还未创建 rigidbody 的时候创建
        if (!_isCreated && _rigidbodyBackendHandle == 0)
        {
            RequestCreate();
        }
    }

    void Rigidbody::OnDisable()
    {
        Component::OnDisable();
        if (_rigidbodyBackendHandle != 0)
        {
            RequestDestroy();
        }
    }

    void Rigidbody::OnDestroy()
    {
        if (_rigidbodyBackendHandle != 0)
        {
            RequestDestroy();
        }
    }

    void Rigidbody::RequestCreate()
    {
        // TODO: 加入对 collider 的形状描述参数
        createDesc.ownerId = GetOwner() ? GetOwner()->GetID() : 0;
        Physics::PhysicsSystem::Instance().EnqueueRigidbodyCreate(createDesc);
        _isCreated = true;
    }

    void Rigidbody::RequestDestroy()
    {
        Physics::PhysicsSystem::Instance().EnqueueRigidbodyDestroy(_rigidbodyBackendHandle);
        // reset statement
        _rigidbodyBackendHandle = 0;
        _isCreated = false;
    }

    void Rigidbody::ApplyImpulse(const Math::Vector3& impulse)
    {
        if (_rigidbodyBackendHandle == 0)
            return;
        Physics::PhysicsSystem::Instance().ApplyImpulse(_rigidbodyBackendHandle, impulse);
    }
    void Rigidbody::SetLinearVelocity(const ChikaEngine::Math::Vector3& vel)
    {
        if (_rigidbodyBackendHandle == 0)
            return;
        Physics::PhysicsSystem::Instance().SetLinearVelocity(_rigidbodyBackendHandle, vel);
    }
} // namespace ChikaEngine::Framework