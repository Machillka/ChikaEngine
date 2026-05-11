#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/PhysicsScene.h"
#include <memory>
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/scene/scene.hpp"

namespace ChikaEngine::Framework
{
    PhysicsSubsystem::PhysicsSubsystem(Scene* scene) : _ownerScene(scene)
    {

        Physics::PhysicsSystemDesc createInfo{};

        _physics = std::make_unique<Physics::PhysicsScene>(createInfo);
    }

    Physics::PhysicsScene* PhysicsSubsystem::GetPhysicsInstace()
    {
        return _physics.get();
    }

    void PhysicsSubsystem::Tick(float dt)
    {
        _physics->Tick(dt);
    }

    bool PhysicsSubsystem::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, Physics::RaycastHit& outHit)
    {
        auto isHit = _physics->Raycast(origin, direction, maxDistance, outHit);
        return isHit;
    }

    Physics::PhysicsBodyHandle PhysicsSubsystem::CreateBodyImmediate(const Physics::PhysicsBodyCreateDesc& desc)
    {
        auto handle = _physics->CreateBodyImmediate(desc);
        return handle;
    }

    void PhysicsSubsystem::SetLinearVelocity(Physics::PhysicsBodyHandle handle, const Math::Vector3 v)
    {
        _physics->SetLinearVelocity(handle, v);
    }

    void PhysicsSubsystem::ApplyImpulse(Physics::PhysicsBodyHandle handle, const Math::Vector3 impulse)
    {
        _physics->ApplyImpulse(handle, impulse);
    }

    void PhysicsSubsystem::SyncTransform()
    {
        // FIXME: 非常的不现代, 效率极低
        auto physicsTransforms = _physics->PollTransform();
        // LOG_INFO("Physics Subsystem", "size of padding: {}, id = {}", physicsTransforms.size(), physicsTransforms[0].first);

        for (auto const& [goId, physicsTransform] : physicsTransforms)
        {
            auto go = _ownerScene->GetGameObject(goId);

            if (!go)
                continue;

            // LOG_INFO("Sync Transform", "ID = {}, originY: {}, phyY: {}", goId, go->transform->position.y, physicsTransform.pos.y);
            go->transform->position = physicsTransform.pos;
            go->transform->rotation = physicsTransform.rot;
        }
    }
} // namespace ChikaEngine::Framework