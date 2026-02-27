#pragma once

#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/subsystem/ISubsystem.h"
#include <memory>
namespace ChikaEngine::Framework
{
    class Scene;
    class PhysicsSubsystem : ISubsystem
    {
      public:
        void Initialize(Scene* scene) override;
        Physics::PhysicsScene* GetPhysicsInstace();
        void Tick(float dt) override;
        void Cleanup() override;

      private:
        Scene* _scene = nullptr;
        std::unique_ptr<Physics::PhysicsScene> _physics = nullptr;
    };
} // namespace ChikaEngine::Framework