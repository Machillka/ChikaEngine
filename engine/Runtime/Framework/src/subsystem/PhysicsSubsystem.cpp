#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/PhysicsScene.h"
#include <memory>
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"

namespace ChikaEngine::Framework
{
    PhysicsSubsystem::PhysicsSubsystem(Scene* scene) : _ownerScene(scene)
    {

        Physics::PhysicsSystemDesc createInfo{};

        _physics = std::make_unique<Physics::PhysicsScene>(createInfo);
        if (!_physics->IsInitialized())
        {
            const Physics::PhysicsResult& result = _physics->GetInitializationResult();
            LOG_ERROR("Physics Subsystem", "Failed to initialize physics scene: {}", result.diagnostic);
        }
    }

    Physics::PhysicsScene* PhysicsSubsystem::GetPhysicsInstace()
    {
        return _physics.get();
    }

    void PhysicsSubsystem::Tick(float dt)
    {
        CHIKA_PROFILE_SCOPE("Physics.Simulate");
        if (_physics)
            _physics->Tick(dt);
    }

    void PhysicsSubsystem::Cleanup()
    {
        if (_physics)
        {
            _physics->Shutdown();
            _physics.reset();
        }
    }

    bool PhysicsSubsystem::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, Physics::RaycastHit& outHit)
    {
        return _physics && _physics->Raycast(origin, direction, maxDistance, outHit);
    }

    Physics::PhysicsBodyCreateResult PhysicsSubsystem::CreateBodyImmediate(const Physics::PhysicsBodyCreateDesc& desc)
    {
        if (!_physics)
            return { .result = Physics::PhysicsResult::Failure(Physics::PhysicsStatus::NotInitialized, "Physics subsystem is not initialized") };
        return _physics->CreateBodyImmediate(desc);
    }

    bool PhysicsSubsystem::SetLinearVelocity(Physics::PhysicsBodyHandle handle, const Math::Vector3& velocity)
    {
        return _physics && _physics->SetLinearVelocity(handle, velocity);
    }

    bool PhysicsSubsystem::ApplyImpulse(Physics::PhysicsBodyHandle handle, const Math::Vector3& impulse)
    {
        return _physics && _physics->ApplyImpulse(handle, impulse);
    }

    void PhysicsSubsystem::SyncTransform()
    {
        CHIKA_PROFILE_SCOPE("Physics.SyncTransforms");
        if (!_physics)
            return;

        // FIXME: 非常的不现代, 效率极低
        auto physicsTransforms = _physics->PollTransform();
        // LOG_INFO("Physics Subsystem", "size of padding: {}, id = {}", physicsTransforms.size(), physicsTransforms[0].first);

        for (auto const& [goId, physicsTransform] : physicsTransforms)
        {
            auto go = _ownerScene->GetGameObject(goId);

            if (!go)
                continue;

            // LOG_INFO("Sync Transform", "ID = {}, originY: {}, phyY: {}", goId, go->transform->position.y, physicsTransform.pos.y);
            go->transform->SetWorldPositionAndRotation(physicsTransform.pos, physicsTransform.rot);
        }
    }
} // namespace ChikaEngine::Framework
