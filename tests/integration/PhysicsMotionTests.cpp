#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/FixedStepAccumulator.hpp"
#include "ChikaEngine/component/Collider.hpp"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.hpp"

#include <cmath>
#include <iostream>

namespace
{
    namespace Core = ChikaEngine::Core;
    namespace Framework = ChikaEngine::Framework;
    namespace Math = ChikaEngine::Math;
    namespace Physics = ChikaEngine::Physics;

    constexpr float FixedDeltaTime = 1.0f / 60.0f;
    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    void Check(const Physics::PhysicsResult& result, const char* message)
    {
        Check(result.Succeeded(), message);
    }

    bool NearlyEqual(float lhs, float rhs, float epsilon = 2.0e-3f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    Physics::PhysicsScene MakeScene()
    {
        Physics::PhysicsSystemDesc desc;
        desc.initDesc.gravity = Math::Vector3::zero;
        desc.initDesc.workerThreadCount = 1;
        return Physics::PhysicsScene(desc);
    }

    Physics::PhysicsBodyCreateDesc MakeBody(Core::GameObjectID owner, Physics::MotionType motionType, Math::Vector3 position = Math::Vector3::zero)
    {
        Physics::PhysicsBodyCreateDesc desc;
        desc.ownerId = owner;
        desc.motionType = motionType;
        desc.position = position;
        desc.shapeDesc.type = Physics::ColliderShapeType::Box;
        desc.shapeDesc.halfExtents = { 0.5f, 0.5f, 0.5f };
        desc.linearDamping = 0.0f;
        desc.angularDamping = 0.0f;
        desc.allowSleeping = false;
        return desc;
    }

    float SimulateOneSecondAtFrameRate(int framesPerSecond)
    {
        auto scene = MakeScene();
        auto body = scene.CreateBodyImmediate(MakeBody(100 + framesPerSecond, Physics::MotionType::Dynamic));
        Check(body.Succeeded(), "frame-rate fixture creates Dynamic body");
        Check(scene.QueueSetLinearVelocity(Physics::PhysicsBodyTarget::FromHandle(body.handle), { 1.0f, 0.0f, 0.0f }), "frame-rate fixture queues velocity");

        Core::FixedStepAccumulator accumulator(FixedDeltaTime, 8);
        for (int frame = 0; frame < framesPerSecond; ++frame)
            accumulator.Consume(1.0f / static_cast<float>(framesPerSecond), [&scene](float dt) { scene.Tick(dt); });

        const auto snapshot = scene.GetBodySnapshot(body.handle);
        Check(snapshot.has_value(), "frame-rate fixture exposes cached main-thread snapshot");
        return snapshot ? snapshot->transform.pos.x : 0.0f;
    }

    void TestFrameRateIndependence()
    {
        const float at30 = SimulateOneSecondAtFrameRate(30);
        const float at60 = SimulateOneSecondAtFrameRate(60);
        const float at144 = SimulateOneSecondAtFrameRate(144);
        Check(NearlyEqual(at30, at60) && NearlyEqual(at60, at144), "30/60/144 FPS produce the same fixed-step trajectory");
    }

    void TestMotionCommandsAndSleep()
    {
        auto scene = MakeScene();
        Physics::PhysicsBodyCreateDesc desc = MakeBody(201, Physics::MotionType::Dynamic);
        desc.mass = 2.0f;
        desc.allowSleeping = true;
        const auto body = scene.CreateBodyImmediate(desc);
        Check(body.Succeeded(), "motion command fixture creates Dynamic body");

        Check(scene.QueueAddForce(Physics::PhysicsBodyTarget::FromHandle(body.handle), { 2.0f, 0.0f, 0.0f }), "force command is queued");
        scene.Tick(FixedDeltaTime);
        auto snapshot = scene.GetBodySnapshot(body.handle);
        Check(snapshot && NearlyEqual(snapshot->linearVelocity.x, FixedDeltaTime, 4.0e-3f), "force uses newtons over one fixed step");

        Check(scene.QueueApplyImpulse(Physics::PhysicsBodyTarget::FromHandle(body.handle), { 2.0f, 0.0f, 0.0f }), "impulse command is queued");
        Check(scene.QueueAddTorque(Physics::PhysicsBodyTarget::FromHandle(body.handle), { 0.0f, 0.0f, 1.0f }), "torque command is queued");
        Check(scene.QueueApplyAngularImpulse(Physics::PhysicsBodyTarget::FromHandle(body.handle), { 0.0f, 0.0f, 1.0f }), "angular impulse command is queued");
        scene.Tick(FixedDeltaTime);
        snapshot = scene.GetBodySnapshot(body.handle);
        Check(snapshot && snapshot->linearVelocity.x > 1.0f && snapshot->angularVelocity.z > 0.0f, "impulse, torque and angular impulse update cached velocities");

        Check(scene.QueueSetBodyActive(Physics::PhysicsBodyTarget::FromHandle(body.handle), false), "sleep command is queued");
        scene.Tick(FixedDeltaTime);
        snapshot = scene.GetBodySnapshot(body.handle);
        Check(snapshot && snapshot->sleeping && scene.PollActiveDynamicSnapshots().empty(), "sleeping Dynamic body is absent from regular transform upload");

        Check(scene.QueueSetBodyActive(Physics::PhysicsBodyTarget::FromHandle(body.handle), true), "wake command is queued");
        scene.Tick(FixedDeltaTime);
        snapshot = scene.GetBodySnapshot(body.handle);
        Check(snapshot && !snapshot->sleeping && !scene.PollActiveDynamicSnapshots().empty(), "WakeUp restores active Dynamic uploads");

        Physics::PhysicsBodyCreateDesc lockedDesc = MakeBody(202, Physics::MotionType::Dynamic, { 20.0f, 0.0f, 0.0f });
        lockedDesc.axisLockMask = Physics::PhysicsAxisLockTranslationX;
        const auto locked = scene.CreateBodyImmediate(lockedDesc);
        Check(locked.Succeeded(), "axis-lock fixture creates Dynamic body");
        Check(scene.QueueApplyImpulse(Physics::PhysicsBodyTarget::FromHandle(locked.handle), { 10.0f, 0.0f, 0.0f }), "axis-lock fixture queues impulse");
        scene.Tick(FixedDeltaTime);
        const auto lockedSnapshot = scene.GetBodySnapshot(locked.handle);
        Check(lockedSnapshot && NearlyEqual(lockedSnapshot->transform.pos.x, 20.0f) && NearlyEqual(lockedSnapshot->linearVelocity.x, 0.0f), "translation axis lock rejects motion on the locked axis");
    }

    void TestKinematicTargetAndDynamicTeleport()
    {
        auto scene = MakeScene();
        const auto kinematic = scene.CreateBodyImmediate(MakeBody(301, Physics::MotionType::Kinematic, { -2.0f, 0.0f, 0.0f }));
        const auto dynamic = scene.CreateBodyImmediate(MakeBody(302, Physics::MotionType::Dynamic, { 0.0f, 0.0f, 0.0f }));
        Check(kinematic && dynamic, "kinematic/teleport fixture creates bodies");

        Check(scene.QueueKinematicTarget(Physics::PhysicsBodyTarget::FromHandle(kinematic.handle), { -0.75f, 0.0f, 0.0f }, Math::Quaternion::Identity()), "kinematic target command is queued");
        scene.Tick(FixedDeltaTime);
        scene.Tick(FixedDeltaTime);
        const auto pushed = scene.GetBodySnapshot(dynamic.handle);
        Check(pushed && pushed->transform.pos.x > 0.0f, "Kinematic target motion pushes a Dynamic body");

        Check(scene.QueueTeleport(Physics::PhysicsBodyTarget::FromHandle(dynamic.handle), { 10.0f, 2.0f, 0.0f }, Math::Quaternion::Identity(), true, Physics::PhysicsWakePolicy::Wake), "teleport command is queued");
        scene.Tick(FixedDeltaTime);
        const auto teleported = scene.GetBodySnapshot(dynamic.handle);
        Check(teleported && NearlyEqual(teleported->transform.pos.x, 10.0f, 0.1f) && NearlyEqual(teleported->linearVelocity.x, 0.0f), "Dynamic teleport applies world pose and resetVelocity policy");
    }

    void TestFrameworkAuthorityAndRenderInterpolation()
    {
        Framework::Scene scene;
        Framework::SceneCreateInfo createInfo;
        createInfo.fixedDeltaTime = FixedDeltaTime;
        createInfo.createDefaultScene = false;
        scene.Initialize(createInfo);
        const Core::GameObjectID id = scene.CreateGameobject("AuthorityBody");
        auto* object = scene.GetGameObject(id);
        object->AddComponent<Framework::Collider>();
        auto* rigidbody = object->AddComponent<Framework::Rigidbody>();
        rigidbody->SetGravityFactor(0.0f);
        rigidbody->SetLinearDamping(0.0f);

        const Core::GameObjectID staticId = scene.CreateGameobject("StaticAuthority");
        auto* staticObject = scene.GetGameObject(staticId);
        staticObject->transform->position = { 20.0f, 0.0f, 0.0f };
        staticObject->AddComponent<Framework::Collider>();

        const Core::GameObjectID kinematicId = scene.CreateGameobject("KinematicAuthority");
        auto* kinematicObject = scene.GetGameObject(kinematicId);
        kinematicObject->transform->position = { 30.0f, 0.0f, 0.0f };
        kinematicObject->AddComponent<Framework::Collider>();
        auto* kinematicBody = kinematicObject->AddComponent<Framework::Rigidbody>();
        kinematicBody->SetMotionType(Physics::MotionType::Kinematic);

        Check(scene.StartPlayMode(), "authority fixture enters Play mode");
        scene.Tick(FixedDeltaTime);

        const Physics::PhysicsBodyHandle oldStaticHandle = scene.GetPhysicsSubsystem()->GetBodyHandle(staticId);
        staticObject->transform->position.x = 21.0f;
        scene.Tick(FixedDeltaTime);
        Check(scene.GetPhysicsSubsystem()->GetBodyHandle(staticId) != oldStaticHandle, "Static Transform edit rebuilds the backend body at PreStep");

        Check(kinematicBody->MoveKinematic({ 31.0f, 0.0f, 0.0f }, Math::Quaternion::Identity()), "Kinematic body accepts an explicit gameplay target");
        scene.Tick(FixedDeltaTime);
        Physics::RaycastHit kinematicHit;
        Check(scene.GetPhysicsSubsystem()->Raycast({ 31.0f, 3.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 5.0f, kinematicHit) && kinematicHit.gameObjectId == kinematicId, "Kinematic target reaches the backend and remains gameplay-authoritative");

        kinematicObject->transform->position.x = 32.0f;
        scene.Tick(FixedDeltaTime);
        Check(scene.GetPhysicsSubsystem()->Raycast({ 32.0f, 3.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 5.0f, kinematicHit) && kinematicHit.gameObjectId == kinematicId, "direct Kinematic Transform edit is converted into a target command");

        object = scene.GetGameObject(id);
        rigidbody = object->GetComponent<Framework::Rigidbody>();
        rigidbody->SetLinearVelocity({ 1.0f, 0.0f, 0.0f });
        scene.Tick(FixedDeltaTime);
        const float physicsX = object->transform->GetWorldPosition().x;
        object->transform->position.x = 100.0f;
        scene.Tick(FixedDeltaTime);
        Check(object->transform->GetWorldPosition().x < 1.0f && object->transform->GetWorldPosition().x > physicsX, "direct Dynamic Transform write is rejected without one-frame authority jitter");

        const float currentX = object->transform->GetWorldPosition().x;
        scene.Tick(FixedDeltaTime * 0.5f);
        const Math::Mat4 renderMatrix = scene.GetRenderWorldMatrix(*object->transform);
        Check(renderMatrix(0, 3) < currentX, "render interpolation reads previous/current pose without feeding back to gameplay Transform");

        const bool queuedTeleport = rigidbody->Teleport({ 5.0f, 0.0f, 0.0f }, Math::Quaternion::Identity());
        Check(queuedTeleport, "Rigidbody exposes explicit Dynamic teleport");
        scene.Tick(FixedDeltaTime);
        Check(NearlyEqual(object->transform->GetWorldPosition().x, 5.0f, 0.1f), "explicit Dynamic teleport becomes authoritative after PreStep");

        scene.Tick(1.0f);
        const Framework::PhysicsTimingStatistics timing = scene.GetPhysicsTimingStatistics();
        Check(timing.lastStepCount == timing.maxStepsPerFrame && timing.lastDroppedTime > 0.0f && timing.totalDroppedTime >= timing.lastDroppedTime, "Scene exposes fixed-step clamp and dropped-time metrics");
    }

    void TestParentScaleValidation()
    {
        Framework::GameObject parent(401, "ScaledParent");
        Framework::GameObject child(402, "PhysicsChild");
        parent.transform->scale = { 2.0f, 1.0f, 1.0f };
        Check(child.transform->SetParent(parent.transform), "parent-scale fixture creates hierarchy");
        auto* collider = child.AddComponent<Framework::Collider>();
        collider->OnValidate();
        Check(!collider->GetAuthoringDiagnostic().empty(), "parent non-uniform scale is rejected instead of silently distorting physics shape");
    }
} // namespace

int main()
{
    Core::UIDGenerator::Instance().Init(31);
    TestFrameRateIndependence();
    TestMotionCommandsAndSleep();
    TestKinematicTargetAndDynamicTeleport();
    TestFrameworkAuthorityAndRenderInterpolation();
    TestParentScaleValidation();

    if (g_failures == 0)
    {
        std::cout << "Physics motion checks passed\n";
        return 0;
    }
    std::cerr << g_failures << " Physics motion check(s) failed\n";
    return 1;
}
