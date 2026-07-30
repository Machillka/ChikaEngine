#include "ChikaEngine/PhysicsCommandBuffer.hpp"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Collider.hpp"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/scene/scene.hpp"

#include <cstddef>
#include <iostream>
#include <vector>

namespace
{
    namespace Physics = ChikaEngine::Physics;
    namespace Framework = ChikaEngine::Framework;

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

    void Check(const Physics::PhysicsBodyCreateResult& result, const char* message)
    {
        Check(result.Succeeded(), message);
    }

    Physics::PhysicsBodyCreateDesc MakeBody(ChikaEngine::Core::GameObjectID ownerId, Physics::MotionType motionType = Physics::MotionType::Dynamic)
    {
        Physics::PhysicsBodyCreateDesc desc;
        desc.ownerId = ownerId;
        desc.motionType = motionType;
        desc.position = { 0, 2, 0 };
        return desc;
    }

    void CheckTrace(const std::vector<Physics::PhysicsCommandExecutionRecord>& trace, const std::vector<Physics::PhysicsCommandType>& expected, const char* message)
    {
        if (trace.size() != expected.size())
        {
            Check(false, message);
            return;
        }
        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            if (trace[index].type != expected[index])
            {
                Check(false, message);
                return;
            }
        }
    }

    void TestRegistryTransactionsAndCommandOrdering()
    {
        Physics::PhysicsSystemDesc systemDesc;
        systemDesc.initDesc.gravity = { 0, 0, 0 };
        systemDesc.initDesc.workerThreadCount = 1;
        Physics::PhysicsScene scene(systemDesc);
        Check(scene.IsInitialized(), "physics lifecycle Scene initializes");
        if (!scene.IsInitialized())
            return;

        const auto bodyA = MakeBody(101);
        Check(scene.QueueCreateBody(bodyA), "create command queues");
        Check(scene.QueueSetLinearVelocity(Physics::PhysicsBodyTarget::FromOwner(bodyA.ownerId), { 1, 0, 0 }), "owner velocity queues before Body exists");
        Check(scene.GetStatistics().pendingCommands == 2 && scene.GetStatistics().activeBodies == 0, "deferred create remains pending until PreStep");
        scene.Tick(1.0f / 60.0f);

        const Physics::PhysicsBodyHandle firstHandle = scene.GetBodyHandle(bodyA.ownerId);
        const auto firstRecord = scene.GetBodyRecord(firstHandle);
        Check(firstHandle && firstRecord && firstRecord->backendToken && firstRecord->colliderHandle && firstRecord->ownerId == bodyA.ownerId && firstRecord->motionType == Physics::MotionType::Dynamic && firstRecord->active, "registry stores engine handle, backend token, owner, Collider identity, motion type and active state");
        Check(scene.GetStatistics().activeBodies == 1 && scene.GetStatistics().backendBodies == 1 && scene.GetStatistics().pendingCommands == 0, "Scene and backend body counts match after deferred create");
        CheckTrace(scene.GetLastCommandExecutionTrace(), { Physics::PhysicsCommandType::Create, Physics::PhysicsCommandType::Velocity }, "create executes before velocity regardless of enqueue dependency");

        const std::uint64_t failuresBeforeDuplicate = scene.GetStatistics().failedCommands;
        Check(scene.QueueCreateBody(bodyA), "duplicate owner create reaches deterministic PreStep validation");
        scene.Tick(1.0f / 60.0f);
        Check(scene.GetBodyHandle(bodyA.ownerId) == firstHandle && scene.GetStatistics().activeBodies == 1 && scene.GetStatistics().backendBodies == 1, "duplicate owner create cannot register a second Body");
        Check(scene.GetStatistics().failedCommands == failuresBeforeDuplicate + 1 && scene.GetLastCommandExecutionTrace().front().status == Physics::PhysicsStatus::DuplicateOwner, "duplicate owner failure is observable in statistics and trace");

        Physics::PhysicsBodyCreateDesc invalidCreate = MakeBody(104);
        invalidCreate.shapeDesc.type = Physics::ColliderShapeType::Capsule;
        const std::size_t activeBeforeInvalidCreate = scene.GetStatistics().activeBodies;
        Check(scene.QueueCreateBody(invalidCreate), "invalid create command queues for backend validation");
        scene.Tick(1.0f / 60.0f);
        Check(!scene.GetBodyHandle(invalidCreate.ownerId) && scene.GetStatistics().activeBodies == activeBeforeInvalidCreate && scene.GetStatistics().backendBodies == activeBeforeInvalidCreate, "failed create does not commit a Handle or backend Body");

        Physics::PhysicsBodyCreateDesc invalidRebuild = bodyA;
        invalidRebuild.shapeDesc.type = Physics::ColliderShapeType::Capsule;
        Check(scene.QueueRebuildBody(invalidRebuild), "invalid rebuild command queues");
        scene.Tick(1.0f / 60.0f);
        Check(scene.GetBodyHandle(bodyA.ownerId) == firstHandle && scene.HasBody(firstHandle), "failed create-new phase preserves the previous Body");

        Physics::PhysicsBodyCreateDesc validRebuild = bodyA;
        validRebuild.position = { 4, 2, 0 };
        Check(scene.QueueRebuildBody(validRebuild), "valid rebuild command queues");
        scene.Tick(1.0f / 60.0f);
        const Physics::PhysicsBodyHandle rebuiltHandle = scene.GetBodyHandle(bodyA.ownerId);
        Check(rebuiltHandle && rebuiltHandle != firstHandle && !scene.HasBody(firstHandle) && scene.HasBody(rebuiltHandle), "successful rebuild retires old Handle and commits a new generation");
        const auto rebuiltRecord = scene.GetBodyRecord(rebuiltHandle);
        Check(rebuiltRecord && rebuiltRecord->colliderHandle == firstRecord->colliderHandle, "rebuild preserves Collider identity while replacing Body identity");
        Check(scene.GetStatistics().activeBodies == 1 && scene.GetStatistics().backendBodies == 1, "rebuild transaction never leaves duplicate active bodies");

        const auto bodyB = MakeBody(102);
        Check(scene.CreateBodyImmediate(bodyB), "test-only immediate create prepares order fixture");
        Check(scene.QueueSetLinearVelocity(Physics::PhysicsBodyTarget::FromHandle(rebuiltHandle), { 2, 0, 0 }), "velocity queues before destroy");
        Check(scene.QueueTeleport(Physics::PhysicsBodyTarget::FromHandle(rebuiltHandle), { 8, 2, 0 }, { 0, 0, 0, 1 }), "teleport queues before destroy");
        Check(scene.QueueRebuildBody(bodyB), "rebuild queues after movement commands");
        Check(scene.QueueDestroyBody(rebuiltHandle), "destroy queues last");
        const std::uint64_t staleBeforeOrderedStep = scene.GetStatistics().staleCommands;
        scene.Tick(1.0f / 60.0f);
        CheckTrace(scene.GetLastCommandExecutionTrace(), { Physics::PhysicsCommandType::Destroy, Physics::PhysicsCommandType::Rebuild, Physics::PhysicsCommandType::Teleport, Physics::PhysicsCommandType::Velocity }, "commands execute in Destroy, Create/Rebuild, Transform, Velocity/Force order");
        Check(scene.GetStatistics().staleCommands == staleBeforeOrderedStep + 2, "commands targeting a Body destroyed earlier in the same PreStep are counted as stale");

        const auto kinematic = MakeBody(103, Physics::MotionType::Kinematic);
        Check(scene.QueueCreateBody(kinematic), "kinematic create queues");
        Check(scene.QueueKinematicTarget(Physics::PhysicsBodyTarget::FromOwner(kinematic.ownerId), { 3, 2, 0 }, { 0, 0, 0, 1 }), "owner kinematic target queues before create is committed");
        scene.Tick(1.0f / 60.0f);
        CheckTrace(scene.GetLastCommandExecutionTrace(), { Physics::PhysicsCommandType::Create, Physics::PhysicsCommandType::KinematicTarget }, "kinematic target executes after create in the same PreStep");

        const Physics::PhysicsBodyHandle bodyBHandle = scene.GetBodyHandle(bodyB.ownerId);
        Check(scene.QueueAddForce(Physics::PhysicsBodyTarget::FromHandle(bodyBHandle), { 0, 5, 0 }), "force command queues");
        Check(scene.QueueApplyImpulse(Physics::PhysicsBodyTarget::FromHandle(bodyBHandle), { 0, 1, 0 }), "impulse command queues");
        scene.Tick(1.0f / 60.0f);
        CheckTrace(scene.GetLastCommandExecutionTrace(), { Physics::PhysicsCommandType::Force, Physics::PhysicsCommandType::Impulse }, "force and impulse preserve stable enqueue order inside the final phase");

        Check(scene.QueueDestroyBody(bodyBHandle), "destroy queues for stale command fixture");
        Check(scene.QueueTeleport(Physics::PhysicsBodyTarget::FromHandle(bodyBHandle), { 0, 0, 0 }, { 0, 0, 0, 1 }), "teleport queues before handle becomes stale");
        Check(scene.QueueSetLinearVelocity(Physics::PhysicsBodyTarget::FromHandle(bodyBHandle), { 0, 0, 0 }), "velocity queues before handle becomes stale");
        Check(scene.QueueApplyImpulse(Physics::PhysicsBodyTarget::FromHandle(bodyBHandle), { 0, 1, 0 }), "impulse queues before handle becomes stale");
        const std::uint64_t staleBeforeDestroy = scene.GetStatistics().staleCommands;
        scene.Tick(1.0f / 60.0f);
        Check(scene.GetStatistics().staleCommands == staleBeforeDestroy + 3 && !scene.HasBody(bodyBHandle), "destroyed Handle safely rejects later transform, velocity and impulse commands");

        const auto stressBody = MakeBody(500);
        Check(scene.QueueCreateBody(stressBody), "stress Body create queues");
        scene.Tick(1.0f / 60.0f);
        const std::uint64_t coalescedBeforeStress = scene.GetStatistics().coalescedCommands;
        for (int iteration = 0; iteration < 1000; ++iteration)
        {
            Check(scene.QueueDestroyBody(stressBody.ownerId), "stress destroy queues");
            Check(scene.QueueRebuildBody(stressBody), "stress rebuild queues");
        }
        scene.Tick(1.0f / 60.0f);
        Check(scene.GetStatistics().coalescedCommands >= coalescedBeforeStress + 1999, "1000 disable/enable intents coalesce to the final structural command");
        Check(scene.GetBodyHandle(stressBody.ownerId) && scene.GetStatistics().activeBodies == scene.GetStatistics().backendBodies, "stress lifecycle leaves exactly one registry/backend Body for the owner");

        Check(scene.QueueSetLinearVelocity(Physics::PhysicsBodyTarget::FromOwner(stressBody.ownerId), { 1, 0, 0 }), "pending command exists before reset");
        const Physics::PhysicsBodyHandle stressHandle = scene.GetBodyHandle(stressBody.ownerId);
        scene.ResetSceneState();
        scene.ResetSceneState();
        const Physics::PhysicsSceneStatistics resetStats = scene.GetStatistics();
        Check(resetStats.activeBodies == 0 && resetStats.backendBodies == 0 && resetStats.pendingCommands == 0 && !scene.HasBody(stressHandle), "idempotent Scene reset clears Body, backend, pending command and stale Handle state");
    }

    void TestCommandCapacityStatistics()
    {
        Physics::PhysicsSystemDesc systemDesc;
        systemDesc.initDesc.gravity = { 0, 0, 0 };
        systemDesc.initDesc.workerThreadCount = 1;
        systemDesc.commandQueueCapacity = 4;
        Physics::PhysicsScene scene(systemDesc);
        const auto desc = MakeBody(601);
        const Physics::PhysicsBodyCreateResult created = scene.CreateBodyImmediate(desc);
        Check(created, "capacity fixture creates Body");
        for (int index = 0; index < 4; ++index)
            Check(scene.QueueSetLinearVelocity(Physics::PhysicsBodyTarget::FromHandle(created.handle), { static_cast<float>(index), 0, 0 }), "command fits configured capacity");
        const Physics::PhysicsResult overflow = scene.QueueSetLinearVelocity(Physics::PhysicsBodyTarget::FromHandle(created.handle), { 5, 0, 0 });
        const Physics::PhysicsSceneStatistics stats = scene.GetStatistics();
        Check(overflow.status == Physics::PhysicsStatus::QueueFull && stats.commandCapacity == 4 && stats.pendingCommands == 4 && stats.peakPendingCommands == 4 && stats.failedCommands >= 1, "queue capacity, peak, rejection and failure statistics are observable");
    }

    void TestFrameworkRigidbodyLifecycle()
    {
        Framework::Scene scene;
        scene.Initialize({});
        const ChikaEngine::Core::GameObjectID ownerId = scene.CreateGameobject("PhysicsLifecycleOwner");
        Framework::GameObject* owner = scene.GetGameObject(ownerId);
        auto* collider = owner ? owner->AddComponent<Framework::Collider>() : nullptr;
        auto* rigidbody = owner ? owner->AddComponent<Framework::Rigidbody>() : nullptr;
        Check(collider && rigidbody, "Framework fixture creates Collider and Rigidbody");
        Check(scene.GetPhysicsSubsystem()->GetStatistics().activeBodies == 0 && scene.GetPhysicsSubsystem()->GetStatistics().pendingCommands == 0, "Rigidbody authoring in Edit mode does not create runtime physics state");

        Check(scene.StartPlayMode(), "Framework fixture enters Play mode");
        Check(scene.GetPhysicsSubsystem()->GetStatistics().pendingCommands >= 1, "Collider/Rigidbody Start queues deferred create intent");
        owner->SetActive(false);
        scene.Tick(1.0f / 60.0f);
        Check(scene.GetPhysicsSubsystem()->GetStatistics().activeBodies == 0 && scene.GetPhysicsSubsystem()->GetStatistics().backendBodies == 0, "create followed by disable before PreStep preserves the final disabled intent");
        owner->SetActive(true);
        scene.Tick(1.0f / 30.0f);
        Check(scene.GetPhysicsSubsystem()->GetStatistics().activeBodies == 1 && scene.GetPhysicsSubsystem()->GetStatistics().backendBodies == 1, "first fixed step commits exactly one Rigidbody Body");

        owner = scene.GetGameObject(ownerId);
        rigidbody = owner ? owner->GetComponent<Framework::Rigidbody>() : nullptr;
        Check(owner && rigidbody, "runtime Rigidbody remains available");
        for (int iteration = 0; iteration < 1000; ++iteration)
        {
            owner->SetActive(false);
            owner->SetActive(true);
        }
        scene.Tick(1.0f / 60.0f);
        Check(scene.GetPhysicsSubsystem()->GetStatistics().activeBodies == 1 && scene.GetPhysicsSubsystem()->GetStatistics().backendBodies == 1, "1000 Rigidbody disable/enable cycles leave one active Body");

        for (int iteration = 0; iteration < 1000; ++iteration)
            rigidbody->OnDirty();
        scene.Tick(1.0f / 60.0f);
        Check(scene.GetPhysicsSubsystem()->GetStatistics().activeBodies == 1 && scene.GetPhysicsSubsystem()->GetStatistics().backendBodies == 1, "1000 Rigidbody Dirty rebuild requests leave one active Body");

        owner->SetActive(false);
        scene.Tick(1.0f / 60.0f);
        Check(scene.GetPhysicsSubsystem()->GetStatistics().activeBodies == 0 && scene.GetPhysicsSubsystem()->GetStatistics().backendBodies == 0, "disabled Rigidbody retires its Body at PreStep");
        owner->SetActive(true);
        scene.Tick(1.0f / 60.0f);
        Check(scene.GetPhysicsSubsystem()->GetStatistics().activeBodies == 1, "re-enabled Rigidbody recreates one Body");

        Check(scene.StopPlayMode(), "Framework fixture exits Play mode");
        const Physics::PhysicsSceneStatistics stoppedStats = scene.GetPhysicsSubsystem()->GetStatistics();
        Check(stoppedStats.activeBodies == 0 && stoppedStats.backendBodies == 0 && stoppedStats.pendingCommands == 0, "StopPlay clears all Body and pending command state");

        Check(scene.StartPlayMode(), "second Play session starts");
        Check(scene.StopPlayMode(), "second Play session can stop before the first fixed step");
        const Physics::PhysicsSceneStatistics secondStopStats = scene.GetPhysicsSubsystem()->GetStatistics();
        Check(secondStopStats.activeBodies == 0 && secondStopStats.backendBodies == 0 && secondStopStats.pendingCommands == 0, "StopPlay clears pending create even before simulation");
    }
} // namespace

int main()
{
    ChikaEngine::Reflection::InitAllReflection();
    ChikaEngine::Core::UIDGenerator::Instance().Init(11);

    TestRegistryTransactionsAndCommandOrdering();
    TestCommandCapacityStatistics();
    TestFrameworkRigidbodyLifecycle();

    if (g_failures == 0)
        std::cout << "Physics lifecycle checks passed\n";
    else
        std::cerr << g_failures << " physics lifecycle test(s) failed\n";
    return g_failures == 0 ? 0 : 1;
}
