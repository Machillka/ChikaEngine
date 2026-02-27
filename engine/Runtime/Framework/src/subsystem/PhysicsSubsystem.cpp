#include "ChikaEngine/subsystem/PhysicsSubsystem.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsScene.h"
#include <memory>
namespace ChikaEngine::Framework
{
    void PhysicsSubsystem::Initialize(Scene* scene)
    {
        _scene = scene;
        Physics::PhysicsSystemDesc desc = {
            .backendType = Physics::PhysicsBackendTypes::Jolt,
        };
        // 生成对应实例
        _physics = std::make_unique<Physics::PhysicsScene>(desc);
    }

    void PhysicsSubsystem::Tick(float dt)
    {

        _physics->Tick(dt);
    }
} // namespace ChikaEngine::Framework