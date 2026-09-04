#include "ChikaEngine/PhysicsEvents.hpp"
#include "ChikaEngine/PhysicsScene.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

namespace
{
    namespace Physics = ChikaEngine::Physics;

    constexpr float FIXED_DELTA_TIME = 1.0f / 60.0f;
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

    Physics::PhysicsBodyCreateDesc MakeBox(ChikaEngine::Core::GameObjectID ownerId, Physics::MotionType motionType, const ChikaEngine::Math::Vector3& position, bool isTrigger = false)
    {
        Physics::PhysicsBodyCreateDesc desc;
        desc.ownerId = ownerId;
        desc.motionType = motionType;
        desc.position = position;
        desc.shapeDesc.type = Physics::ColliderShapeType::Box;
        desc.shapeDesc.halfExtents = { 0.5f, 0.5f, 0.5f };
        desc.isTrigger = isTrigger;
        return desc;
    }

    Physics::PhysicsScene MakeScene(const ChikaEngine::Math::Vector3& gravity)
    {
        Physics::PhysicsSystemDesc desc;
        desc.initDesc.gravity = gravity;
        desc.initDesc.workerThreadCount = 1;
        return Physics::PhysicsScene(desc);
    }

    std::size_t CountPhase(const std::vector<Physics::PhysicsPairEvent>& events, Physics::PhysicsPairPhase phase)
    {
        return static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [phase](const Physics::PhysicsPairEvent& event) { return event.phase == phase; }));
    }

    void TestCollisionEnterStayExitAndCanonicalData()
    {
        auto scene = MakeScene({ 0.0f, -9.81f, 0.0f });
        Check(scene.IsInitialized(), "collision contact Scene initializes");
        if (!scene.IsInitialized())
            return;

        Physics::PhysicsBodyCreateDesc floorDesc = MakeBox(101, Physics::MotionType::Static, { 0.0f, 0.0f, 0.0f });
        floorDesc.shapeDesc.halfExtents = { 2.0f, 0.5f, 2.0f };
        const auto floor = scene.CreateBodyImmediate(floorDesc);
        const auto body = scene.CreateBodyImmediate(MakeBox(102, Physics::MotionType::Dynamic, { 0.0f, 0.95f, 0.0f }));
        Check(floor && body, "collision contact bodies are created");
        if (!floor || !body)
            return;

        Check(scene.DrainPairEvents().empty(), "contact events are not published before PhysicsSystem::Update");
        scene.Tick(FIXED_DELTA_TIME);
        const auto enter = scene.DrainPairEvents();
        Check(enter.size() == 1 && CountPhase(enter, Physics::PhysicsPairPhase::Enter) == 1, "first collision step emits exactly one canonical Enter");
        if (!enter.empty())
        {
            const Physics::PhysicsPairEvent& event = enter.front();
            Check(event.kind == Physics::PhysicsPairKind::Collision, "solid Box pair is classified as Collision");
            Check(event.pair.bodyA == floor.handle && event.pair.bodyB == body.handle, "pair identity is sorted by engine Body handle");
            Check(event.gameObjectA == floorDesc.ownerId && event.gameObjectB == 102, "canonical event caches both GameObject identities");
            Check(event.hasContactData && event.contact.hasPoint && event.contact.hasNormal && event.contact.hasPenetration, "collision event exposes explicit manifold validity");
            Check(event.contact.hasRelativeVelocity && !event.contact.hasImpulse, "pre-solver relative velocity is valid while impulse remains unavailable");
            Check(event.contact.normal.y > 0.5f, "canonical normal points from pair A toward pair B");
        }

        for (int step = 0; step < 2; ++step)
        {
            scene.Tick(FIXED_DELTA_TIME);
            const auto stay = scene.DrainPairEvents();
            Check(stay.size() == 1 && CountPhase(stay, Physics::PhysicsPairPhase::Stay) == 1, "persisted collision emits one Stay per fixed step");
            if (!stay.empty())
                Check(stay.front().contact.normal.y > 0.5f, "canonical normal orientation remains stable across Stay");
        }

        Check(scene.QueueTeleport(Physics::PhysicsBodyTarget::FromHandle(body.handle), { 0.0f, 5.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }), "separation teleport queues");
        scene.Tick(FIXED_DELTA_TIME);
        const auto exit = scene.DrainPairEvents();
        Check(exit.size() == 1 && CountPhase(exit, Physics::PhysicsPairPhase::Exit) == 1, "separation emits exactly one Exit");
        if (!exit.empty())
            Check(exit.front().terminationReason == Physics::PhysicsPairTerminationReason::Separated, "separation Exit has an explicit termination reason");
        Check(scene.GetStatistics().activeContactPairs == 0, "separated pair is removed from the Scene contact cache");
    }

    void TestTriggerSequenceAndNoCollisionImpulse()
    {
        auto scene = MakeScene({ 0.0f, 0.0f, 0.0f });
        Check(scene.IsInitialized(), "trigger contact Scene initializes");
        if (!scene.IsInitialized())
            return;

        const auto trigger = scene.CreateBodyImmediate(MakeBox(201, Physics::MotionType::Static, { 0.0f, 0.0f, 0.0f }, true));
        const auto visitor = scene.CreateBodyImmediate(MakeBox(202, Physics::MotionType::Dynamic, { 0.0f, 0.0f, 0.0f }));
        Check(trigger && visitor, "trigger bodies are created");
        if (!trigger || !visitor)
            return;

        scene.Tick(FIXED_DELTA_TIME);
        const auto enter = scene.DrainPairEvents();
        Check(enter.size() == 1 && enter.front().phase == Physics::PhysicsPairPhase::Enter && enter.front().kind == Physics::PhysicsPairKind::Trigger, "sensor pair emits Trigger Enter only");
        if (!enter.empty())
            Check(!enter.front().contact.hasImpulse, "Trigger never claims a solver impulse");

        scene.Tick(FIXED_DELTA_TIME);
        const auto stay = scene.DrainPairEvents();
        Check(stay.size() == 1 && stay.front().phase == Physics::PhysicsPairPhase::Stay && stay.front().kind == Physics::PhysicsPairKind::Trigger, "sensor pair emits Trigger Stay only");

        Check(scene.QueueTeleport(Physics::PhysicsBodyTarget::FromHandle(visitor.handle), { 4.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }), "trigger visitor separation queues");
        scene.Tick(FIXED_DELTA_TIME);
        const auto exit = scene.DrainPairEvents();
        Check(exit.size() == 1 && exit.front().phase == Physics::PhysicsPairPhase::Exit && exit.front().kind == Physics::PhysicsPairKind::Trigger, "sensor pair emits Trigger Exit without a Collision duplicate");
    }

    void TestBodyDestroyExitIsPostStepAndIdentitySafe()
    {
        auto scene = MakeScene({ 0.0f, 0.0f, 0.0f });
        Check(scene.IsInitialized(), "destroy contact Scene initializes");
        if (!scene.IsInitialized())
            return;

        const auto trigger = scene.CreateBodyImmediate(MakeBox(301, Physics::MotionType::Static, { 0.0f, 0.0f, 0.0f }, true));
        const auto visitor = scene.CreateBodyImmediate(MakeBox(302, Physics::MotionType::Dynamic, { 0.0f, 0.0f, 0.0f }));
        scene.Tick(FIXED_DELTA_TIME);
        (void)scene.DrainPairEvents();
        Check(trigger && visitor && scene.GetStatistics().activeContactPairs == 1, "destroy test starts with one active pair");

        Check(scene.QueueDestroyBody(visitor.handle), "contacting Body destroy queues");
        scene.PreStep(FIXED_DELTA_TIME);
        Check(scene.DrainPairEvents().empty(), "Body destroy Exit remains staged until backend Update returns");
        Check(scene.Simulate(FIXED_DELTA_TIME), "post-destroy fixed step simulates");
        const auto exit = scene.DrainPairEvents();
        Check(exit.size() == 1 && exit.front().phase == Physics::PhysicsPairPhase::Exit, "Body destroy emits exactly one canonical Exit");
        if (!exit.empty())
        {
            Check(exit.front().terminationReason == Physics::PhysicsPairTerminationReason::BodyDestroyed, "destroy Exit records BodyDestroyed termination");
            Check(exit.front().gameObjectA == 301 && exit.front().gameObjectB == 302, "destroy Exit uses cached owner identities after registry removal");
        }
        Check(!scene.HasBody(visitor.handle) && scene.GetStatistics().activeContactPairs == 0, "destroy removes Body and contact pair without stale lookup");
        scene.Tick(FIXED_DELTA_TIME);
        Check(scene.DrainPairEvents().empty(), "backend Removed callback cannot duplicate a pre-notified destroy Exit");
    }

    void TestPairOrderingDedupAndSleepSuppression()
    {
        auto sortedScene = MakeScene({ 0.0f, 0.0f, 0.0f });
        const auto triggerA = sortedScene.CreateBodyImmediate(MakeBox(401, Physics::MotionType::Static, { -3.0f, 0.0f, 0.0f }, true));
        const auto visitorA = sortedScene.CreateBodyImmediate(MakeBox(402, Physics::MotionType::Dynamic, { -3.0f, 0.0f, 0.0f }));
        const auto triggerB = sortedScene.CreateBodyImmediate(MakeBox(403, Physics::MotionType::Static, { 3.0f, 0.0f, 0.0f }, true));
        const auto visitorB = sortedScene.CreateBodyImmediate(MakeBox(404, Physics::MotionType::Dynamic, { 3.0f, 0.0f, 0.0f }));
        Check(triggerA && visitorA && triggerB && visitorB, "two isolated trigger pairs are created");
        sortedScene.Tick(FIXED_DELTA_TIME);
        const auto events = sortedScene.DrainPairEvents();
        Check(events.size() == 2 && CountPhase(events, Physics::PhysicsPairPhase::Enter) == 2, "multiple contact points and pairs produce one Enter per canonical pair");
        if (events.size() == 2)
            Check(events[0].pair < events[1].pair, "drain order is stable by phase then canonical pair key");

        auto sleepingScene = MakeScene({ 0.0f, -9.81f, 0.0f });
        Physics::PhysicsBodyCreateDesc floorDesc = MakeBox(501, Physics::MotionType::Static, { 0.0f, 0.0f, 0.0f });
        floorDesc.shapeDesc.halfExtents = { 2.0f, 0.5f, 2.0f };
        const auto floor = sleepingScene.CreateBodyImmediate(floorDesc);
        const auto sleeper = sleepingScene.CreateBodyImmediate(MakeBox(502, Physics::MotionType::Dynamic, { 0.0f, 1.0f, 0.0f }));
        Check(floor && sleeper, "sleep suppression bodies are created");

        bool sawEnter = false;
        bool sawFalseExit = false;
        for (int step = 0; step < 180; ++step)
        {
            sleepingScene.Tick(FIXED_DELTA_TIME);
            for (const Physics::PhysicsPairEvent& event : sleepingScene.DrainPairEvents())
            {
                sawEnter = sawEnter || event.phase == Physics::PhysicsPairPhase::Enter;
                sawFalseExit = sawFalseExit || event.phase == Physics::PhysicsPairPhase::Exit;
            }
        }
        Check(sawEnter, "resting body establishes a contact before sleeping");
        Check(!sawFalseExit, "Jolt sleep/deactivation removal does not become a gameplay Exit");
        Check(sleepingScene.GetStatistics().activeContactPairs == 1, "sleeping contact remains in the canonical pair cache");
        Check(sleepingScene.GetStatistics().suppressedDeactivationExits >= 1, "sleep removal suppression is observable in statistics");
    }
} // namespace

int main()
{
    TestCollisionEnterStayExitAndCanonicalData();
    TestTriggerSequenceAndNoCollisionImpulse();
    TestBodyDestroyExitIsPostStepAndIdentitySafe();
    TestPairOrderingDedupAndSleepSuppression();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " physics contact check(s) failed\n";
        return 1;
    }
    std::cout << "Physics contact state stream checks passed\n";
    return 0;
}
