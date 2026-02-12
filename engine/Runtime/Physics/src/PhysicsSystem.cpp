#include "ChikaEngine/PhysicsSystem.h"
#include "ChikaEngine/PhysicsJoltBackend.h"
#include "ChikaEngine/scene/scene.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <queue>
#include <utility>

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

    void PhysicsSystem::EnqueueRigidbodyCreate(const PhysicsBodyCreateDesc& desc)
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
        LOG_INFO("Physics System", "Tick");

        ProcessCreateRigidbodyQueue();
        ProcessDestroyRigidbodyQueue();

        // 物理计算
        _backend->Simulate(dt);

        // TODO[x]: 更新位置
        // TODO[x]: 实现 scene

        // 做一次 snapshot 防止意外修改数据
        std::vector<std::pair<PhysicsBodyHandle, Framework::GameObjectID>> snapshot;
        snapshot.reserve(_physicsHandleToGO.size());
        for (auto const& kv : _physicsHandleToGO)
            snapshot.emplace_back(kv.first, kv.second);

        LOG_DEBUG("Fuck", "table size = {}", _physicsHandleToGO.size());
        for (auto const& p : snapshot)
        {
            LOG_DEBUG("Fuck", "updating physics");
            PhysicsTransform ts;
            if (_backend->TrySyncTransform(p.first, ts))
            {
                LOG_INFO("Physics", "{}, {}", p.first, p.second);
                if (auto go = Framework::Scene::Instance().GetGOByID(p.second))
                {
                    LOG_INFO("Physics", "Update GO, ID = {}, x = {}, y = {}, z = {}", p.second, ts.pos.x, ts.pos.y, ts.pos.z);
                    go->transform->position = ts.pos;
                    go->transform->rotation = ts.rot;
                }
            }
            else
            {
                // Diagnostic: TrySyncTransform failed for this handle — log once to help debugging
                LOG_INFO("Physics", "TrySyncTransform failed for handle = {}, owner GO = {}", p.first, p.second);
            }
        }

        // TODO: 加入碰撞事件处理
    }

    // NOTE: 是否要改成批处理??
    void PhysicsSystem::SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3 v)
    {
        if (_backend)
            _backend->SetLinearVelocity(handle, v);
    }
    void PhysicsSystem::ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3 impulse)
    {
        if (_backend)
            _backend->ApplyImpulse(handle, impulse);
    }

    void PhysicsSystem::SetLayerMask(std::uint32_t layerIndex, Framework::LayerMask mask)
    {
        if (_backend)
            _backend->SetLayerMask(layerIndex, mask);
    }

    Framework::LayerMask PhysicsSystem::GetLayerMask(std::uint32_t layerIndex) const
    {
        if (_backend)
            return _backend->GetLayerMask(layerIndex);
        return Framework::LayerMask(Framework::GameObjectLayer::Default);
    }

    void PhysicsSystem::RegisterRigidbody(PhysicsBodyHandle handle, Framework::GameObjectID id)
    {
        // NOTE: 允许覆盖
        // auto it = std::find_if(_physicsHandleToGO.begin(), _physicsHandleToGO.end(), [id](const auto& kv) { return kv.second == id; });

        // // 说明重复
        // if (it != _physicsHandleToGO.end())
        //     return;

        _physicsHandleToGO[handle] = id;
    }

    void PhysicsSystem::ProcessCreateRigidbodyQueue()
    {
        std::queue<PhysicsBodyCreateDesc> q;
        {
            std::lock_guard lock(_createRigidbodyMutex);
            std::swap(q, _createRigidbodyQueue);
        }

        while (!q.empty())
        {
            auto desc = q.front();
            q.pop();
            // 去重
            bool ownerHasBody = false;
            for (const auto& kv : _physicsHandleToGO)
            {
                if (kv.second == desc.ownerId)
                {
                    ownerHasBody = true;
                    break;
                }
            }
            if (ownerHasBody)
                continue;
            if (!_backend)
                continue;
            PhysicsBodyHandle handle = _backend->CreateBodyFromDesc(desc);
            if (handle != 0)
            {
                LOG_INFO("Physics System", "Successfully Register, id = {}", desc.ownerId);
                RegisterRigidbody(handle, desc.ownerId);
            }
        }
    }

    void PhysicsSystem::ProcessDestroyRigidbodyQueue()
    {
        std::queue<PhysicsBodyHandle> q;
        {
            std::lock_guard lock(_destroyRigidbodyMutex);
            std::swap(q, _destroyRigidbodyQueue);
        }

        while (!q.empty())
        {
            auto handle = q.front();
            q.pop();

            auto it = _physicsHandleToGO.find(handle);
            if (it != _physicsHandleToGO.end())
                _physicsHandleToGO.erase(it);
            if (_backend)
                _backend->DestroyPhysicsBody(handle);
        }
    }
    PhysicsBodyHandle PhysicsSystem::CreateBodyImmediate(const PhysicsBodyCreateDesc& desc)
    {
        std::lock_guard lock(_createRigidbodyMutex);

        PhysicsBodyHandle handle = _backend->CreateBodyFromDesc(desc);
        if (handle != 0)
        {
            RegisterRigidbody(handle, desc.ownerId);
            LOG_INFO("Physics System", "Create physicsbody successfully, phyid = {}, ownerId = {}", handle, desc.ownerId);
        }
        return handle;
    }
    void PhysicsSystem::SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& pos, const Math::Quaternion& rot) {}
} // namespace ChikaEngine::Physics