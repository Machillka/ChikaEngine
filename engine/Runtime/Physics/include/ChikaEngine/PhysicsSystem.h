#pragma once

#include "ChikaEngine/IPhysicsBackend.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include <queue>
#include <unordered_map>
namespace ChikaEngine::Physics
{
    // 对射线加入GO信息
    struct RaycastHitInfo : public RaycastHit
    {
        Framework::GameObjectID gameObjectId = 0;
        Framework::GameObject* gameObject = nullptr; // 可选，方便直接访问
    };

    // 给 engine 调用的前端
    class PhysicsSystem
    {
      public:
        static PhysicsSystem& Instance();
        bool Initialize(const PhysicsSystemDesc& desc);
        void Shutdown();
        void Tick(float fixedDeltaTime); // 每一帧的物理运算

        // 创建 Rigidbody
        void EnqueueRigidbodyCreate(const PhysicsBodyCreateDesc& desc);
        // 销毁 Rigidbody
        void EnqueueRigidbodyDestroy(PhysicsBodyHandle handle);

        // 提供修改速度和提供瞬时冲量
        void SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3 v);
        void ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3 impulse);

        void SetLayerMask(std::uint32_t layerIndex, Framework::LayerMask mask);
        Framework::LayerMask GetLayerMask(std::uint32_t layerIndex) const;

        // 射线检测, 返回是否击中和击中信息
        bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHitInfo& outHit);

        PhysicsBodyHandle CreateBodyImmediate(const PhysicsBodyCreateDesc& desc);

        // 强制移动 ( Editor 测试用)
        void SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& pos, const Math::Quaternion& rot);

      private:
        PhysicsSystem() = default;
        ~PhysicsSystem() = default;

        // 在 主线程中运行 保证安全
        void ProcessCreateRigidbodyQueue();
        void ProcessDestroyRigidbodyQueue();
        void RegisterRigidbody(PhysicsBodyHandle handle, Framework::GameObjectID id);

        std::unique_ptr<IPhysicsBackend> _backend = nullptr;

        // 线程锁 和 创建队列
        std::mutex _createRigidbodyMutex;
        std::queue<PhysicsBodyCreateDesc> _createRigidbodyQueue;
        std::mutex _destroyRigidbodyMutex;
        std::queue<PhysicsBodyHandle> _destroyRigidbodyQueue;

        // 维护 GO 拥有的 physics 组件
        std::unordered_map<PhysicsBodyHandle, Framework::GameObjectID> _physicsHandleToGO;
    };
} // namespace ChikaEngine::Physics