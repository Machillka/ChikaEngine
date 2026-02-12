#include "ChikaEngine/PhysicsSystem.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsScene.h"
#include <memory>
namespace ChikaEngine::Physics
{

    PhysicsSystem& PhysicsSystem::Instance()
    {
        static PhysicsSystem instance;
        return instance;
    }

    std::unique_ptr<PhysicsScene> PhysicsSystem::CreatePhysicsScene(PhysicsSystemDesc& desc)
    {
        auto physicsScene = std::make_unique<PhysicsScene>(desc);

        return physicsScene;
    }

} // namespace ChikaEngine::Physics