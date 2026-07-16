#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "PhysicsJoltBackend.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/debug/log_macros.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <utility>

namespace ChikaEngine::Physics
{
    PhysicsScene::PhysicsScene(const PhysicsSystemDesc& desc)
    {
        _initializationResult = Initialize(desc);
    }

    PhysicsScene::~PhysicsScene()
    {
        Shutdown();
    }

    PhysicsResult PhysicsScene::Initialize(const PhysicsSystemDesc& desc)
    {
        if (IsInitialized())
        {
            _initializationResult = PhysicsResult{ .status = PhysicsStatus::AlreadyInitialized, .diagnostic = "Physics scene is already initialized" };
            return _initializationResult;
        }

        std::unique_ptr<IPhysicsBackend> candidate;
        switch (desc.backendType)
        {
        case PhysicsBackendType::None:
            _initializationResult = PhysicsResult::Failure(PhysicsStatus::UnsupportedBackend, "Physics backend 'None' cannot create a simulation world");
            return _initializationResult;
        case PhysicsBackendType::Jolt:
            candidate = std::make_unique<PhysicsJoltBackend>();
            break;
        }

        if (!candidate)
        {
            _initializationResult = PhysicsResult::Failure(PhysicsStatus::UnsupportedBackend, "Unknown physics backend");
            return _initializationResult;
        }

        PhysicsResult result = candidate->Initialize(desc.initDesc);
        if (!result)
        {
            candidate->Shutdown();
            _initializationResult = std::move(result);
            return _initializationResult;
        }

        _backend = std::move(candidate);
        _initializationResult = std::move(result);
        return _initializationResult;
    }

    void PhysicsScene::Shutdown() noexcept
    {
        if (_backend)
        {
            _backend->Shutdown();
            _backend.reset();
        }
        _physicsHandleToGO.clear();
        _updatedTransforms.clear();
        {
            std::lock_guard lock(_destroyRigidbodyMutex);
            std::queue<PhysicsBodyHandle> empty;
            std::swap(_destroyRigidbodyQueue, empty);
        }
        _initializationResult = PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized");
    }

    bool PhysicsScene::IsInitialized() const noexcept
    {
        return _backend && _backend->IsInitialized();
    }

    const PhysicsResult& PhysicsScene::GetInitializationResult() const noexcept
    {
        return _initializationResult;
    }

    PhysicsBackendCapabilities PhysicsScene::GetCapabilities() const noexcept
    {
        return _backend ? _backend->GetCapabilities() : PhysicsBackendCapabilities{};
    }

    bool PhysicsScene::Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, RaycastHit& outHit)
    {
        outHit = {};
        if (!IsInitialized())
            return false;

        if (_backend->Raycast(origin, direction, maxDistance, outHit))
        {
            // 通过后端返回的 BodyHandle，查找对应的 GameObjectID
            auto it = _physicsHandleToGO.find(outHit.bodyHandle);
            if (it != _physicsHandleToGO.end())
            {
                outHit.gameObjectId = it->second;
                return true;
            }
        }
        outHit = {};
        return false;
    }

    bool PhysicsScene::EnqueueRigidbodyDestroy(PhysicsBodyHandle handle)
    {
        if (!HasBody(handle))
            return false;
        std::lock_guard lock(_destroyRigidbodyMutex);
        _destroyRigidbodyQueue.push(handle);
        return true;
    }

    void PhysicsScene::Tick(float dt)
    {
        if (!IsInitialized())
            return;
        ProcessDestroyRigidbodyQueue();
        (void)_backend->Simulate(dt);
    }

    const std::vector<std::pair<Core::GameObjectID, PhysicsTransform>>& PhysicsScene::PollTransform()
    {
        _updatedTransforms.clear();
        if (!IsInitialized())
            return _updatedTransforms;

        std::vector<std::pair<PhysicsBodyHandle, Core::GameObjectID>> snapshot;
        snapshot.reserve(_physicsHandleToGO.size());
        for (auto const& kv : _physicsHandleToGO)
            snapshot.emplace_back(kv.first, kv.second);
        for (auto const& p : snapshot)
        {
            PhysicsTransform ts;
            if (_backend->TrySyncTransform(p.first, ts))
            {
                _updatedTransforms.push_back(std::make_pair(p.second, ts));
            }
        }

        return _updatedTransforms;
    }

    bool PhysicsScene::SetLinearVelocity(PhysicsBodyHandle handle, const Math::Vector3& velocity)
    {
        return IsInitialized() && _backend->SetLinearVelocity(handle, velocity);
    }

    bool PhysicsScene::ApplyImpulse(PhysicsBodyHandle handle, const Math::Vector3& impulse)
    {
        return IsInitialized() && _backend->ApplyImpulse(handle, impulse);
    }

    bool PhysicsScene::SetLayerCollisionMask(PhysicsLayerID layerId, PhysicsLayerMask mask)
    {
        return IsInitialized() && _backend->SetLayerCollisionMask(layerId, mask);
    }

    PhysicsLayerMask PhysicsScene::GetLayerCollisionMask(PhysicsLayerID layerId) const
    {
        if (IsInitialized())
            return _backend->GetLayerCollisionMask(layerId);
        return 0;
    }

    void PhysicsScene::RegisterRigidbody(PhysicsBodyHandle handle, Core::GameObjectID id)
    {
        _physicsHandleToGO[handle] = id;
    }

    void PhysicsScene::ProcessDestroyRigidbodyQueue()
    {
        std::queue<PhysicsBodyHandle> q;
        {
            std::lock_guard lock(_destroyRigidbodyMutex);
            std::swap(q, _destroyRigidbodyQueue);
        }

        while (!q.empty())
        {
            const PhysicsBodyHandle handle = q.front();
            q.pop();

            if (_backend && _backend->DestroyPhysicsBody(handle))
                _physicsHandleToGO.erase(handle);
        }
    }

    PhysicsBodyCreateResult PhysicsScene::CreateBodyImmediate(const PhysicsBodyCreateDesc& desc)
    {
        std::lock_guard lock(_createRigidbodyMutex);
        if (!IsInitialized())
            return { .result = PhysicsResult::Failure(PhysicsStatus::NotInitialized, "Physics scene is not initialized") };

        PhysicsBodyCreateResult createResult = _backend->CreateBodyFromDesc(desc);
        if (!createResult)
        {
            LOG_ERROR("Physics", "Failed to create body for owner {}: {}", desc.ownerId, createResult.result.diagnostic);
            return createResult;
        }

        RegisterRigidbody(createResult.handle, desc.ownerId);
        return createResult;
    }

    bool PhysicsScene::SetBodyTransform(PhysicsBodyHandle handle, const Math::Vector3& pos, const Math::Quaternion& rot)
    {
        return IsInitialized() && _backend->SetBodyTransform(handle, pos, rot);
    }

    bool PhysicsScene::HasBody(PhysicsBodyHandle handle) const
    {
        return IsInitialized() && _backend->HasBody(handle);
    }
} // namespace ChikaEngine::Physics
