#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/debug/Gizmo.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/scene/scene.hpp"

namespace ChikaEngine::Framework
{
    Scene* Rigidbody::GetSceneSave()
    {
        auto owner = GetOwner();
        if (!owner)
            return nullptr;
        auto scene = owner->GetScene();
        if (!scene)
            return nullptr;
        return scene;
    }

    void Rigidbody::CreateRigidbody()
    {
        Physics::PhysicsBodyCreateDesc createInfo{};

        auto owner = GetOwner();

        if (!owner)
        {
            LOG_WARN("Rigidbody Component", "Did not have a owner gameobject");
            return;
        }

        createInfo.ownerId = owner->GetID();
        LOG_INFO("Rigidbody", "Gameobject ID = {}", owner->GetID());
        createInfo.position = owner->transform->position;
        createInfo.rotation = owner->transform->rotation;
        createInfo.shapeDesc = Physics::ColliderShapeDesc{
            .type = Physics::RigidbodyShapes::Box,
            .center = _colliderCenter,
            .halfExtents = Math::Vector3{ 0.5f, 0.5f, 0.5f },
            .radius = _colliderRadius,
            .height = _colliderHeight,
        };
        createInfo.friction = _friction;
        createInfo.mass = _mass;
        auto scene = owner->GetScene();

        if (!scene)
        {
            LOG_WARN("Rigidbody Component", "Did not have a owner scene");
            return;
        }

        _physicsHandle = scene->GetPhysicsSubsystem()->CreateBodyImmediate(createInfo);
        if (_physicsHandle)
            LOG_INFO("Rigidbody", "Create Rigidbody handle {} successfully!", _physicsHandle);
    }

    void Rigidbody::Awake()
    {
        if (_need2RecreateRigidbody)
            CreateRigidbody();
        _need2RecreateRigidbody = false;
    }

    void Rigidbody::OnDirty()
    {
        CreateRigidbody();
    }

    // TODO: 跳过 Rigidbody 运算, 位移等移交给 transform
    void Rigidbody::OnEnable()
    {
        // _isEnabled = true;

        // if (_need2RecreateRigidbody)
        //     CreateRigidbody();
    }

    void Rigidbody::OnDisable()
    {
        _isEnabled = false;

        auto scene = GetSceneSave();
        if (!scene)
            return;
        // TODO: 提供直接销毁方法
        scene->GetPhysicsSubsystem()->EnqueueRigidbodyDestroy(_physicsHandle);
        _physicsHandle = 0;
        _need2RecreateRigidbody = true;
    }

    void Rigidbody::OnGizmo() const
    {
        auto owner = GetOwner();
        if (!owner || !owner->transform)
            return;

        // 1. 计算 Collider 的世界中心点 (Transform 位置 + 旋转后的 Offset)
        Math::Vector3 worldCenter = owner->transform->position + owner->transform->rotation.Rotate(_colliderCenter);

        // 2. 获取当前的旋转
        Math::Quaternion worldRot = owner->transform->rotation;

        // 3. 计算实际的 HalfExtents (基础大小 0.5 * Transform缩放)
        // 假设当前默认是 Box，后续可以根据实际形状区分 DrawWireSphere 等
        Math::Vector3 halfExtents(0.5f, 0.5f, 0.5f);
        halfExtents.x *= owner->transform->scale.x;
        halfExtents.y *= owner->transform->scale.y;
        halfExtents.z *= owner->transform->scale.z;

        // 4. 调用全局 Gizmo 绘制亮绿色线框
        Debug::Gizmo::DrawWireBox(worldCenter, halfExtents, worldRot, { 0.2f, 1.0f, 0.2f, 1.0f });
    }

    void Rigidbody::SetLinearVelocity(Math::Vector3 v)
    {
        auto scene = GetSceneSave();
        if (!scene)
            return;

        scene->GetPhysicsSubsystem()->SetLinearVelocity(_physicsHandle, v);
    }
    void Rigidbody::Impulse(Math::Vector3 impulse)
    {
        auto scene = GetSceneSave();
        if (!scene)
            return;
        scene->GetPhysicsSubsystem()->ApplyImpulse(_physicsHandle, impulse);
    }

} // namespace ChikaEngine::Framework