#pragma once

#include "ChikaEngine/IPhysicsBackend.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "PhysicsDescs.h"
#include <queue>
#include <unordered_map>
namespace ChikaEngine::Physics
{
    // 下沉到可以拥有多个实例——不同 scene 进行物理模拟
    class PhysicsScene
    {
      public:
        PhysicsScene(const PhysicsSystemDesc& desc);
        ~PhysicsScene() = default;

        bool Initialize(const PhysicsSystemDesc& desc);
        void Shutdown();
        void Tick(float fixedDeltaTime); // 每一帧的物理运算

        // 创建 Rigidbody
        // void EnqueueRigidbodyCreate(const PhysicsBodyCreateDesc& desc);
        // 销毁 Rigidbody
        void EnqueueRigidbodyDestroy(PhysicsBodyHandle handle);

        // 提供修改速度和提供瞬时冲量
        void SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3 v);
        void ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3 impulse);

        // 射线检测, 返回是否击中和击中信息
        bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit);

        PhysicsBodyHandle CreateBodyImmediate(const PhysicsBodyCreateDesc& desc);

        // 强制移动 ( Editor 测试用)
        void SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& pos, const Math::Quaternion& rot);

        // 获得更新后的物理数据,交付给 Scene 来执行更新逻辑
        const std::vector<std::pair<Core::GameObjectID, PhysicsTransform>>& PollTransform();

        void SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask);
        PhysicsLayerMask GetLayerCollisionMask(PhysicsLayerID layerId);

      private:
        // 在 主线程中运行 保证安全
        // void ProcessCreateRigidbodyQueue();
        void ProcessDestroyRigidbodyQueue();
        void RegisterRigidbody(PhysicsBodyHandle handle, Core::GameObjectID id);

        std::unique_ptr<IPhysicsBackend> _backend = nullptr;

        // 线程锁 和 创建队列
        std::mutex _createRigidbodyMutex;
        // std::queue<PhysicsBodyCreateDesc> _createRigidbodyQueue;
        std::mutex _destroyRigidbodyMutex;
        std::queue<PhysicsBodyHandle> _destroyRigidbodyQueue;

        // 维护 GO 拥有的 physics 组件
        std::unordered_map<PhysicsBodyHandle, Core::GameObjectID> _physicsHandleToGO;
        // 维护更新数据 每次创建麻烦并且垂悬, 可以使用指针返回
        std::vector<std::pair<Core::GameObjectID, PhysicsTransform>> _updatedTransforms;
    };
} // namespace ChikaEngine::Physics