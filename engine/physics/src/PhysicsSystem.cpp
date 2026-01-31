#include "include/PhysicsSystem.h"

#include "debug/log_macros.h"
#include "include/PhysicsDescs.h"
#include "include/PhysicsJoltBackend.h"

#include <memory>
#include <mutex>

namespace ChikaEngine::Physics
{

    PhysicsSystem& PhysicsSystem::Instance()
    {
        static PhysicsSystem instance;
        return instance;
    }
    bool PhysicsSystem::Initialize(const PhysicsSystemDesc& desc)
    {
        switch (desc.backendType)
        {
        case PhysicsBackendTypes::None:
            LOG_ERROR("Physics", "No supported backend");
            return false;
        case PhysicsBackendTypes::Jolt:
            _backend = std::make_unique<PhysicsJoltBackend>();
        }

        return _backend->Initialize(desc.initDesc);
    }

    void PhysicsSystem::Shutdown()
    {
        if (_backend)
        {
            _backend->Shutdown();
            _backend.reset();
        }
        _physicsHandleToGO.clear();
    }

    void PhysicsSystem::EnqueueRigidbodyCreate(const RigidbodyCreateDesc& desc)
    {
        std::lock_guard lock(_createRigidbodyMutex);
        _createRigidbodyQueue.push(desc);
    }

    void PhysicsSystem::EnqueueRigidbodyDestroy(PhysicsBodyHandle handle)
    {
        std::lock_guard lock(_destroyRigidbodyMutex);
        _destroyRigidbodyQueue.push(handle);
    }

    void PhysicsSystem::Tick(float dt)
    {
        ProcessCreateRigidbodyQueue();
        ProcessDestroyRigidbodyQueue();

        // 物理计算
        _backend->Simulate(dt);

        // TODO: 更新位置
        // TODO: 实现 scene
    }
} // namespace ChikaEngine::Physics