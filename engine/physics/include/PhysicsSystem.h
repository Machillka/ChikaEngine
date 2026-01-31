#pragma once

#include "GameObject.h"
#include "include/IPhysicsBackend.h"
#include "include/PhysicsDescs.h"

#include <memory>
#include <queue>
#include <unordered_map>
namespace ChikaEngine::Physics
{
    // 引擎可调用的
    class PhysicsSystem
    {
      public:
        static PhysicsSystem& Instance();
        bool Initialize(const PhysicsSystemDesc& desc);
        void Shutdown();
        void Tick(float dt); // 每一帧的物理运算

        // 创建 Rigidbody
        void EnqueueRigidbodyCreate(const RigidbodyCreateDesc& desc);
        // 销毁 Rigidbody
        void EnqueueRigidbodyDestroy(PhysicsBodyHandle handle);

      private:
        PhysicsSystem() = default;
        ~PhysicsSystem() = default;

        // 在 主线程中运行
        void ProcessCreateRigidbodyQueue();
        void ProcessDestroyRigidbodyQueue();

        std::unique_ptr<IPhysicsBackend> _backend = nullptr;

        // 线程锁 和 创建队列
        std::mutex _createRigidbodyMutex;
        std::queue<RigidbodyCreateDesc> _createRigidbodyQueue;
        std::mutex _destroyRigidbodyMutex;
        std::queue<PhysicsBodyHandle> _destroyRigidbodyQueue;

        // 维护 GO 拥有的 physics 组件
        std::unordered_map<PhysicsBodyHandle, Framework::GameObjectID> _physicsHandleToGO;
    };
} // namespace ChikaEngine::Physics