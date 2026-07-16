#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/debug/Gizmo.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/scene/scene.hpp"

namespace ChikaEngine::Framework
{
    Rigidbody::~Rigidbody()
    {
        OnDestroy();
    }

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

    bool Rigidbody::QueueRigidbodyCreateOrRebuild()
    {
        Physics::PhysicsBodyCreateDesc createInfo{};

        auto owner = GetOwner();

        if (!owner)
        {
            LOG_WARN("Rigidbody Component", "Did not have a owner gameobject");
            return false;
        }

        createInfo.ownerId = owner->GetID();
        createInfo.position = owner->transform->GetWorldPosition();
        createInfo.rotation = owner->transform->GetWorldRotation();
        createInfo.shapeDesc = Physics::ColliderShapeDesc{
            .type = Physics::ColliderShapeType::Box,
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
            return false;
        }

        auto* physics = scene->GetPhysicsSubsystem();
        if (!physics)
        {
            LOG_WARN("Rigidbody Component", "Owner scene does not have an initialized physics subsystem");
            return false;
        }

        const Physics::PhysicsBodyHandle activeHandle = physics->GetBodyHandle(owner->GetID());
        const Physics::PhysicsResult result = activeHandle ? physics->QueueRebuildBody(createInfo) : physics->QueueCreateBody(createInfo);
        if (result)
        {
            _physicsHandle = activeHandle;
            return true;
        }

        LOG_ERROR("Rigidbody", "Failed to queue body lifecycle command: {}", result.diagnostic);
        return false;
    }

    void Rigidbody::Awake()
    {
        _need2RecreateRigidbody = true;
    }

    void Rigidbody::Start()
    {
        if (_need2RecreateRigidbody)
            _need2RecreateRigidbody = !QueueRigidbodyCreateOrRebuild();
    }

    void Rigidbody::FixedTick(float)
    {
        RefreshPhysicsHandle();
    }

    void Rigidbody::OnDirty()
    {
        _need2RecreateRigidbody = true;
        Scene* scene = GetSceneSave();
        if (IsActiveAndEnabled() && scene && (scene->IsPlaying() || scene->IsPaused()))
        {
            _need2RecreateRigidbody = !QueueRigidbodyCreateOrRebuild();
        }
    }

    // TODO: 跳过 Rigidbody 运算, 位移等移交给 transform
    void Rigidbody::OnEnable()
    {
        Scene* scene = GetSceneSave();
        if (_need2RecreateRigidbody && scene && (scene->IsPlaying() || scene->IsPaused()))
            _need2RecreateRigidbody = !QueueRigidbodyCreateOrRebuild();
    }

    void Rigidbody::OnDisable()
    {
        Scene* scene = GetSceneSave();
        auto owner = GetOwner();
        if (scene && owner && scene->GetPhysicsSubsystem() && (scene->IsPlaying() || scene->IsPaused()))
            (void)scene->GetPhysicsSubsystem()->QueueDestroyBody(owner->GetID());
        _physicsHandle = Physics::PhysicsBodyHandle::Invalid();
        _need2RecreateRigidbody = true;
    }

    void Rigidbody::OnDestroy()
    {
        auto scene = GetSceneSave();
        auto owner = GetOwner();
        _physicsHandle = Physics::PhysicsBodyHandle::Invalid();
        if (!scene || !owner || !scene->GetPhysicsSubsystem())
            return;

        if (scene->IsPlaying() || scene->IsPaused())
            (void)scene->GetPhysicsSubsystem()->QueueDestroyBody(owner->GetID());
    }

    void Rigidbody::RefreshPhysicsHandle()
    {
        auto scene = GetSceneSave();
        auto owner = GetOwner();
        if (!scene || !owner || !scene->GetPhysicsSubsystem())
        {
            _physicsHandle = Physics::PhysicsBodyHandle::Invalid();
            return;
        }
        _physicsHandle = scene->GetPhysicsSubsystem()->GetBodyHandle(owner->GetID());
    }

    void Rigidbody::OnGizmo() const
    {
        auto owner = GetOwner();
        if (!owner || !owner->transform)
            return;

        // 1. 计算 Collider 的世界中心点 (Transform 位置 + 旋转后的 Offset)
        Math::Vector3 worldCenter = owner->transform->GetWorldPosition() + owner->transform->GetWorldRotation().Rotate(_colliderCenter);

        // 2. 获取当前的旋转
        Math::Quaternion worldRot = owner->transform->GetWorldRotation();

        // 3. 计算实际的 HalfExtents (基础大小 0.5 * Transform缩放)
        // 假设当前默认是 Box，后续可以根据实际形状区分 DrawWireSphere 等
        Math::Vector3 halfExtents(0.5f, 0.5f, 0.5f);
        const auto worldScale = owner->transform->GetWorldScale();
        halfExtents.x *= worldScale.x;
        halfExtents.y *= worldScale.y;
        halfExtents.z *= worldScale.z;

        // 4. 调用全局 Gizmo 绘制亮绿色线框
        Debug::Gizmo::DrawWireBox(worldCenter, halfExtents, worldRot, { 0.2f, 1.0f, 0.2f, 1.0f });
    }

    void Rigidbody::SetLinearVelocity(Math::Vector3 v)
    {
        auto scene = GetSceneSave();
        auto owner = GetOwner();
        if (!scene || !owner || !scene->GetPhysicsSubsystem())
            return;

        (void)scene->GetPhysicsSubsystem()->QueueSetLinearVelocity(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), v);
    }
    void Rigidbody::AddForce(Math::Vector3 force)
    {
        auto scene = GetSceneSave();
        auto owner = GetOwner();
        if (!scene || !owner || !scene->GetPhysicsSubsystem())
            return;

        (void)scene->GetPhysicsSubsystem()->QueueAddForce(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), force);
    }
    void Rigidbody::Impulse(Math::Vector3 impulse)
    {
        auto scene = GetSceneSave();
        auto owner = GetOwner();
        if (!scene || !owner || !scene->GetPhysicsSubsystem())
            return;
        (void)scene->GetPhysicsSubsystem()->QueueApplyImpulse(Physics::PhysicsBodyTarget::FromOwner(owner->GetID()), impulse);
    }

} // namespace ChikaEngine::Framework
