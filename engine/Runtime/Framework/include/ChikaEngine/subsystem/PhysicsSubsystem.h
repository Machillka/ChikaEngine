#pragma once

#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/subsystem/ISubsystem.h"
#include <memory>
#include <unordered_map>
namespace ChikaEngine::Framework
{
    class Scene;
    class Transform;
    class PhysicsSubsystem : ISubsystem
    {
      public:
        PhysicsSubsystem(Scene* scene);
        // void Initialize(Scene* scene) override;
        Physics::PhysicsScene* GetPhysicsInstace();
        void Tick(float dt) override;
        void Cleanup() override;
        void ResetSceneState();
        /** @brief Converts authoring Transform edits into motion-type-specific commands before simulation. */
        void PrepareTransforms(float fixedDeltaTime);
        void SetRenderInterpolationAlpha(float alpha);
        [[nodiscard]] Math::Mat4 GetRenderWorldMatrix(const Transform& transform) const;

      public:
        bool Raycast(const Math::Vector3& origin, const Math::Vector3& direction, float maxDistance, Physics::RaycastHit& outHit);
        Physics::PhysicsResult QueueCreateBody(const Physics::PhysicsBodyCreateDesc& desc);
        Physics::PhysicsResult QueueRebuildBody(const Physics::PhysicsBodyCreateDesc& desc);
        Physics::PhysicsResult QueueDestroyBody(Core::GameObjectID ownerId);

        bool SetLinearVelocity(Physics::PhysicsBodyHandle handle, const Math::Vector3& velocity);
        bool ApplyForce(Physics::PhysicsBodyHandle handle, const Math::Vector3& force, Physics::PhysicsWakePolicy wakePolicy = Physics::PhysicsWakePolicy::Wake);
        bool ApplyImpulse(Physics::PhysicsBodyHandle handle, const Math::Vector3& impulse);

        void SyncTransform();
        /** @brief Projects and dispatches post-step contact events on the Scene main thread. */
        void DispatchEvents();

      private:
        struct TransformAuthorityState
        {
            Physics::PhysicsBodyHandle handle = Physics::PhysicsBodyHandle::Invalid();
            Physics::MotionType motionType = Physics::MotionType::Static;
            Physics::PhysicsTransform transform;
            Math::Vector3 worldScale{ 1.0f, 1.0f, 1.0f };
            bool initialized = false;
        };

        struct InterpolationState
        {
            Physics::PhysicsBodyHandle handle = Physics::PhysicsBodyHandle::Invalid();
            Physics::PhysicsTransform previous;
            Physics::PhysicsTransform current;
        };

        Scene* _ownerScene = nullptr;
        std::unique_ptr<Physics::PhysicsScene> _physics = nullptr;
        std::unordered_map<Core::GameObjectID, TransformAuthorityState> _authorityStates;
        std::unordered_map<Core::GameObjectID, InterpolationState> _interpolationStates;
        float _renderInterpolationAlpha = 1.0f;
    };
} // namespace ChikaEngine::Framework
