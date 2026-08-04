#include "ChikaEngine/PhysicsCallbackEvents.hpp"
#include "ChikaEngine/PhysicsScene.h"
#include "ChikaEngine/ScriptSystem.h"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/component/ScriptComponent.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/scene/scene.hpp"

#include <pybind11/pybind11.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    namespace Core = ChikaEngine::Core;
    namespace Framework = ChikaEngine::Framework;
    namespace Math = ChikaEngine::Math;
    namespace Physics = ChikaEngine::Physics;
    namespace Scripts = ChikaEngine::Scripts;
    namespace py = pybind11;

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

    const char* PhaseName(Physics::PhysicsPairPhase phase)
    {
        switch (phase)
        {
        case Physics::PhysicsPairPhase::Enter:
            return "enter";
        case Physics::PhysicsPairPhase::Stay:
            return "stay";
        case Physics::PhysicsPairPhase::Exit:
            return "exit";
        }
        return "unknown";
    }

    class RecordingReceiver final : public Framework::Component
    {
      public:
        RecordingReceiver(std::string label, std::vector<Framework::PhysicsContactEvent>* events, std::vector<std::string>* order = nullptr) : _label(std::move(label)), _events(events), _order(order) {}

        void OnCollisionEnter(const Framework::PhysicsContactEvent& event) override
        {
            Record(event);
        }
        void OnCollisionStay(const Framework::PhysicsContactEvent& event) override
        {
            Record(event);
        }
        void OnCollisionExit(const Framework::PhysicsContactEvent& event) override
        {
            Record(event);
        }
        void OnTriggerEnter(const Framework::PhysicsContactEvent& event) override
        {
            Record(event);
        }
        void OnTriggerStay(const Framework::PhysicsContactEvent& event) override
        {
            Record(event);
        }
        void OnTriggerExit(const Framework::PhysicsContactEvent& event) override
        {
            Record(event);
        }

      private:
        void Record(const Framework::PhysicsContactEvent& event)
        {
            if (_events)
                _events->push_back(event);
            if (_order)
                _order->push_back(_label + ":" + PhaseName(event.phase));
        }

        std::string _label;
        std::vector<Framework::PhysicsContactEvent>* _events = nullptr;
        std::vector<std::string>* _order = nullptr;
    };

    class ConfiguredScriptComponent final : public Framework::ScriptComponent
    {
      public:
        explicit ConfiguredScriptComponent(std::string configuredClass)
        {
            moduleName = "physics_broadcast_probe";
            className = std::move(configuredClass);
        }
    };

    class MutatingReceiver final : public Framework::Component
    {
      public:
        MutatingReceiver(Core::GameObjectID otherId, Framework::Component** receiverToRemove, std::vector<Framework::PhysicsContactEvent>* events) : _otherId(otherId), _receiverToRemove(receiverToRemove), _events(events) {}

        void OnTriggerEnter(const Framework::PhysicsContactEvent& event) override
        {
            _events->push_back(event);
            if (_mutated)
                return;
            _mutated = true;
            if (_receiverToRemove && *_receiverToRemove)
                GetOwner()->RemoveComponent(*_receiverToRemove);
            Framework::Scene* scene = GetOwner()->GetScene();
            scene->DestroyGameObject(_otherId);
            Check(scene->GetPhysicsSubsystem()->QueueDestroyBody(_otherId), "mutation callback queues the other Body destroy");
        }

        void OnTriggerExit(const Framework::PhysicsContactEvent& event) override
        {
            _events->push_back(event);
        }

      private:
        Core::GameObjectID _otherId = Core::InvalidGameObjectID;
        Framework::Component** _receiverToRemove = nullptr;
        std::vector<Framework::PhysicsContactEvent>* _events = nullptr;
        bool _mutated = false;
    };

    class SelfDestroyingReceiver final : public Framework::Component
    {
      public:
        explicit SelfDestroyingReceiver(std::vector<Framework::PhysicsContactEvent>* events) : _events(events) {}

        void OnCollisionEnter(const Framework::PhysicsContactEvent& event) override
        {
            _events->push_back(event);
            if (_destroyedSelf)
                return;
            _destroyedSelf = true;
            Framework::Scene* scene = GetOwner()->GetScene();
            const Core::GameObjectID ownerId = GetOwner()->GetID();
            Check(scene->DestroyGameObject(ownerId), "self-destroy callback marks its owner for destruction");
            Check(scene->GetPhysicsSubsystem()->QueueDestroyBody(ownerId), "self-destroy callback queues its Body destroy");
        }

      private:
        std::vector<Framework::PhysicsContactEvent>* _events = nullptr;
        bool _destroyedSelf = false;
    };

    struct PairBodies
    {
        Physics::PhysicsBodyCreateResult bodyA;
        Physics::PhysicsBodyCreateResult bodyB;
    };

    PairBodies CreatePair(Framework::Scene& scene, Core::GameObjectID ownerA, Core::GameObjectID ownerB, bool trigger)
    {
        Physics::PhysicsBodyCreateDesc descA;
        descA.ownerId = ownerA;
        descA.motionType = Physics::MotionType::Static;
        descA.isTrigger = trigger;
        descA.position = trigger ? Math::Vector3{ 0.0f, 3.0f, 0.0f } : Math::Vector3{ 0.0f, 0.0f, 0.0f };
        descA.shapeDesc.halfExtents = trigger ? Math::Vector3{ 0.75f, 0.75f, 0.75f } : Math::Vector3{ 2.0f, 0.5f, 2.0f };

        Physics::PhysicsBodyCreateDesc descB;
        descB.ownerId = ownerB;
        descB.motionType = Physics::MotionType::Dynamic;
        descB.position = trigger ? Math::Vector3{ 0.0f, 3.0f, 0.0f } : Math::Vector3{ 0.0f, 0.95f, 0.0f };

        Physics::PhysicsScene* physics = scene.GetPhysicsSubsystem();
        PairBodies bodies{ .bodyA = physics->CreateBodyImmediate(descA), .bodyB = physics->CreateBodyImmediate(descB) };
        if (!bodies.bodyA)
            std::cerr << "Body A create failed: " << bodies.bodyA.result.diagnostic << '\n';
        if (!bodies.bodyB)
            std::cerr << "Body B create failed: " << bodies.bodyB.result.diagnostic << '\n';
        return bodies;
    }

    void SeparatePair(Framework::Scene& scene, const PairBodies& pair, bool trigger)
    {
        const Math::Vector3 position = trigger ? Math::Vector3{ 4.0f, 3.0f, 0.0f } : Math::Vector3{ 0.0f, 5.0f, 0.0f };
        Check(scene.GetPhysicsSubsystem()->QueueTeleport(Physics::PhysicsBodyTarget::FromHandle(pair.bodyB.handle), position, { 0.0f, 0.0f, 0.0f, 1.0f }), "pair separation teleport queues");
    }

    void TestPairBroadcast(bool trigger)
    {
        Framework::Scene scene;
        scene.Initialize({ .fixedDeltaTime = FIXED_DELTA_TIME, .maxPhysicsStepsPerFrame = 4, .createDefaultScene = false, .useEditorView = false });
        const Core::GameObjectID ownerA = scene.CreateGameobject(trigger ? "TriggerA" : "CollisionA");
        const Core::GameObjectID ownerB = scene.CreateGameobject(trigger ? "TriggerB" : "CollisionB");
        Check(scene.StartPlayMode(), "broadcast Scene enters Play mode");

        std::vector<Framework::PhysicsContactEvent> eventsA;
        std::vector<Framework::PhysicsContactEvent> eventsB;
        std::vector<Framework::PhysicsContactEvent> disabledEvents;
        std::vector<std::string> order;
        scene.GetGameObject(ownerA)->AddComponent<RecordingReceiver>("A", &eventsA, &order);
        scene.GetGameObject(ownerB)->AddComponent<RecordingReceiver>("B", &eventsB, &order);
        auto* disabled = scene.GetGameObject(ownerA)->AddComponent<RecordingReceiver>("disabled", &disabledEvents, &order);
        disabled->SetEnabled(false);

        std::vector<Physics::PhysicsPairEvent> observerEvents;
        scene.GetEventBus().Subscribe<Physics::PhysicsPairEvent>(
            [&](const Physics::PhysicsPairEvent& event)
            {
                observerEvents.push_back(event);
                order.push_back(std::string("observer:") + PhaseName(event.phase));
            });
        std::size_t selfRemovingObserverCount = 0;
        Framework::EventBus::SubscriptionId selfRemovingObserver = 0;
        selfRemovingObserver = scene.GetEventBus().Subscribe<Physics::PhysicsPairEvent>(
            [&](const Physics::PhysicsPairEvent&)
            {
                ++selfRemovingObserverCount;
                scene.GetEventBus().Unsubscribe(selfRemovingObserver);
            });

        const PairBodies pair = CreatePair(scene, ownerA, ownerB, trigger);
        Check(pair.bodyA && pair.bodyB, "broadcast test creates both Bodies");
        scene.Tick(FIXED_DELTA_TIME);
        Check(eventsA.size() == 1 && eventsB.size() == 1 && observerEvents.size() == 1, "Enter reaches A, B and one observer");
        Check(order == std::vector<std::string>{ "A:enter", "B:enter", "observer:enter" }, "dispatch order is A, B, observer");
        Check(disabledEvents.empty(), "disabled receiver is skipped");
        if (!eventsA.empty() && !eventsB.empty())
        {
            Check(eventsA.front().kind == (trigger ? Physics::PhysicsPairKind::Trigger : Physics::PhysicsPairKind::Collision), "owner view preserves pair kind");
            Check(eventsA.front().selfGameObject == ownerA && eventsA.front().otherGameObject == ownerB, "A view has stable self/other IDs");
            Check(eventsB.front().selfGameObject == ownerB && eventsB.front().otherGameObject == ownerA, "B view swaps stable IDs");
            Check(eventsA.front().selfAlive && eventsA.front().otherAlive && eventsB.front().selfAlive && eventsB.front().otherAlive, "live pair reports both participants alive");
            if (eventsA.front().contact.hasNormal && eventsB.front().contact.hasNormal)
                Check(std::abs(eventsA.front().contact.normal.x + eventsB.front().contact.normal.x) < 0.0001f && std::abs(eventsA.front().contact.normal.y + eventsB.front().contact.normal.y) < 0.0001f && std::abs(eventsA.front().contact.normal.z + eventsB.front().contact.normal.z) < 0.0001f, "B view reverses normal");
        }

        Check(scene.PausePlayMode(), "Scene pauses");
        scene.Tick(1.0f);
        Check(eventsA.size() == 1 && eventsB.size() == 1 && observerEvents.size() == 1, "pause does not repeat events");
        Check(scene.ResumePlayMode(), "Scene resumes");
        scene.Tick(FIXED_DELTA_TIME);
        Check(eventsA.size() == 2 && eventsA.back().phase == Physics::PhysicsPairPhase::Stay && eventsB.size() == 2 && observerEvents.size() == 2, "resume emits Stay");

        SeparatePair(scene, pair, trigger);
        scene.Tick(FIXED_DELTA_TIME);
        Check(eventsA.size() == 3 && eventsA.back().phase == Physics::PhysicsPairPhase::Exit, "A receives Exit");
        Check(eventsB.size() == 3 && eventsB.back().phase == Physics::PhysicsPairPhase::Exit, "B receives Exit");
        Check(observerEvents.size() == 3 && observerEvents.back().phase == Physics::PhysicsPairPhase::Exit, "observer receives one Exit");
        Check(selfRemovingObserverCount == 1, "EventBus observer mutation takes effect after the current publish snapshot");
        Check(order == std::vector<std::string>{ "A:enter", "B:enter", "observer:enter", "A:stay", "B:stay", "observer:stay", "A:exit", "B:exit", "observer:exit" }, "all phases retain stable ordering");
    }

    void TestInactiveAndMutation()
    {
        {
            Framework::Scene scene;
            scene.Initialize({ .fixedDeltaTime = FIXED_DELTA_TIME, .maxPhysicsStepsPerFrame = 2, .createDefaultScene = false, .useEditorView = false });
            const auto ownerA = scene.CreateGameobject("ActiveOwner");
            const auto ownerB = scene.CreateGameobject("InactiveOwner");
            Check(scene.StartPlayMode(), "inactive test enters Play");
            std::vector<Framework::PhysicsContactEvent> eventsA;
            std::vector<Framework::PhysicsContactEvent> eventsB;
            scene.GetGameObject(ownerA)->AddComponent<RecordingReceiver>("A", &eventsA);
            scene.GetGameObject(ownerB)->AddComponent<RecordingReceiver>("B", &eventsB);
            scene.GetGameObject(ownerB)->SetActive(false);
            std::size_t observerCount = 0;
            scene.GetEventBus().Subscribe<Physics::PhysicsPairEvent>([&](const auto&) { ++observerCount; });
            const PairBodies pair = CreatePair(scene, ownerA, ownerB, true);
            Check(pair.bodyA && pair.bodyB, "inactive test creates Bodies");
            scene.Tick(FIXED_DELTA_TIME);
            Check(eventsA.size() == 1 && eventsA.front().otherAlive, "active owner sees inactive but alive peer");
            Check(eventsB.empty() && observerCount == 1, "inactive owner is skipped while observer still runs");
        }

        Framework::Scene scene;
        scene.Initialize({ .fixedDeltaTime = FIXED_DELTA_TIME, .maxPhysicsStepsPerFrame = 2, .createDefaultScene = false, .useEditorView = false });
        const auto ownerA = scene.CreateGameobject("MutatingA");
        const auto ownerB = scene.CreateGameobject("DestroyedB");
        Check(scene.StartPlayMode(), "mutation test enters Play");
        std::vector<Framework::PhysicsContactEvent> mutatorEvents;
        std::vector<Framework::PhysicsContactEvent> removedEvents;
        std::vector<Framework::PhysicsContactEvent> eventsB;
        Framework::Component* receiverToRemove = nullptr;
        scene.GetGameObject(ownerA)->AddComponent<MutatingReceiver>(ownerB, &receiverToRemove, &mutatorEvents);
        receiverToRemove = scene.GetGameObject(ownerA)->AddComponent<RecordingReceiver>("removed", &removedEvents);
        scene.GetGameObject(ownerB)->AddComponent<RecordingReceiver>("B", &eventsB);
        std::vector<Physics::PhysicsPairEvent> observerEvents;
        scene.GetEventBus().Subscribe<Physics::PhysicsPairEvent>([&](const auto& event) { observerEvents.push_back(event); });
        const PairBodies pair = CreatePair(scene, ownerA, ownerB, true);
        Check(pair.bodyA && pair.bodyB, "mutation test creates Bodies");

        scene.Tick(FIXED_DELTA_TIME);
        Check(mutatorEvents.size() == 1 && removedEvents.size() == 1, "owner snapshot completes before removal takes effect");
        Check(eventsB.empty() && !scene.GetGameObject(ownerB), "destroyed B is skipped and flushed safely");
        Check(observerEvents.size() == 1, "observer runs after owner mutation");
        scene.Tick(FIXED_DELTA_TIME);
        Check(mutatorEvents.size() == 2 && mutatorEvents.back().phase == Physics::PhysicsPairPhase::Exit, "survivor receives BodyDestroyed Exit");
        Check(!mutatorEvents.back().otherAlive && mutatorEvents.back().otherGameObject == ownerB, "destroy Exit preserves ID and false liveness");
        Check(removedEvents.size() == 1, "removed receiver does not receive later Exit");
        Check(observerEvents.size() == 2 && observerEvents.back().terminationReason == Physics::PhysicsPairTerminationReason::BodyDestroyed, "observer receives one BodyDestroyed Exit");

        Framework::Scene selfDestroyScene;
        selfDestroyScene.Initialize({ .fixedDeltaTime = FIXED_DELTA_TIME, .maxPhysicsStepsPerFrame = 2, .createDefaultScene = false, .useEditorView = false });
        const auto selfOwner = selfDestroyScene.CreateGameobject("SelfDestroyingA");
        const auto survivorOwner = selfDestroyScene.CreateGameobject("SelfDestroySurvivorB");
        Check(selfDestroyScene.StartPlayMode(), "self-destroy test enters Play");
        std::vector<Framework::PhysicsContactEvent> selfEvents;
        std::vector<Framework::PhysicsContactEvent> selfSnapshotFollowerEvents;
        std::vector<Framework::PhysicsContactEvent> survivorEvents;
        selfDestroyScene.GetGameObject(selfOwner)->AddComponent<SelfDestroyingReceiver>(&selfEvents);
        selfDestroyScene.GetGameObject(selfOwner)->AddComponent<RecordingReceiver>("self-follower", &selfSnapshotFollowerEvents);
        selfDestroyScene.GetGameObject(survivorOwner)->AddComponent<RecordingReceiver>("survivor", &survivorEvents);
        const PairBodies selfDestroyPair = CreatePair(selfDestroyScene, selfOwner, survivorOwner, false);
        Check(selfDestroyPair.bodyA && selfDestroyPair.bodyB, "self-destroy test creates Bodies");
        selfDestroyScene.Tick(FIXED_DELTA_TIME);
        Check(selfEvents.size() == 1 && selfSnapshotFollowerEvents.size() == 1, "self destroy preserves the current owner receiver snapshot");
        Check(survivorEvents.size() == 1 && !survivorEvents.front().otherAlive, "survivor receives Enter with destroyed peer liveness refreshed");
        Check(!selfDestroyScene.GetGameObject(selfOwner), "self-destroyed owner flushes safely after dispatch");
        selfDestroyScene.Tick(FIXED_DELTA_TIME);
        Check(survivorEvents.size() == 2 && survivorEvents.back().phase == Physics::PhysicsPairPhase::Exit && !survivorEvents.back().otherAlive, "survivor receives one BodyDestroyed Exit after self destruction");
    }

    void RunScriptSequence(bool trigger)
    {
        py::module_ module = py::module_::import("physics_broadcast_probe");
        module.attr("reset_events")();
        Framework::Scene scene;
        scene.Initialize({ .fixedDeltaTime = FIXED_DELTA_TIME, .maxPhysicsStepsPerFrame = 2, .createDefaultScene = false, .useEditorView = false });
        const auto ownerA = scene.CreateGameobject(trigger ? "ScriptTriggerA" : "ScriptCollisionA");
        const auto ownerB = scene.CreateGameobject(trigger ? "ScriptTriggerB" : "ScriptCollisionB");
        Check(scene.StartPlayMode(), "script test enters Play");
        scene.GetGameObject(ownerA)->AddComponent<ConfiguredScriptComponent>("ThrowingPhysicsProbe");
        scene.GetGameObject(ownerA)->AddComponent<ConfiguredScriptComponent>("PhysicsProbe");
        std::vector<Framework::PhysicsContactEvent> cppEvents;
        scene.GetGameObject(ownerA)->AddComponent<RecordingReceiver>("after-script", &cppEvents);
        const PairBodies pair = CreatePair(scene, ownerA, ownerB, trigger);
        Check(pair.bodyA && pair.bodyB, "script test creates Bodies");
        scene.Tick(FIXED_DELTA_TIME);
        scene.Tick(FIXED_DELTA_TIME);
        SeparatePair(scene, pair, trigger);
        scene.Tick(FIXED_DELTA_TIME);

        py::list events = module.attr("events");
        Check(events.size() == 3 && cppEvents.size() == 3, "script receives all phases and exceptions do not stop later receiver");
        if (events.size() == 3)
        {
            const std::string prefix = trigger ? "on_trigger_" : "on_collision_";
            const py::tuple enter = py::cast<py::tuple>(events[0]);
            const py::tuple stay = py::cast<py::tuple>(events[1]);
            const py::tuple exit = py::cast<py::tuple>(events[2]);
            Check(py::cast<std::string>(enter[0]) == prefix + "enter" && py::cast<std::string>(stay[0]) == prefix + "stay" && py::cast<std::string>(exit[0]) == prefix + "exit", "script callback names match kind and phase");
            const py::dict payload = py::cast<py::dict>(enter[1]);
            Check(py::cast<Core::GameObjectID>(payload["self_game_object_id"]) == ownerA && py::cast<Core::GameObjectID>(payload["other_game_object_id"]) == ownerB, "script payload contains stable IDs");
            Check(py::cast<std::uint64_t>(payload["self_body_handle"]) == pair.bodyA.handle.Value() && py::cast<std::uint64_t>(payload["other_body_handle"]) == pair.bodyB.handle.Value(), "script payload contains value handles");
            Check(py::cast<bool>(payload["self_alive"]) && py::cast<bool>(payload["other_alive"]) && py::cast<std::uint64_t>(payload["fixed_step_index"]) > 0, "script payload contains liveness and step identity");
            Check(py::cast<bool>(payload["payload_readonly"]), "script payload is a read-only value snapshot");
        }
    }
} // namespace

int main()
{
    static_assert(std::is_trivially_copyable_v<Framework::PhysicsContactEvent>);
    ChikaEngine::Reflection::InitAllReflection();
    Core::UIDGenerator::Instance().Init(29);
    if (!Scripts::ScriptsSystem::Instance().Init("tests/integration"))
    {
        std::cerr << "FAILED: Script system did not initialize\n";
        return 1;
    }
    TestPairBroadcast(false);
    TestPairBroadcast(true);
    TestInactiveAndMutation();
    RunScriptSequence(false);
    RunScriptSequence(true);

    Scripts::ScriptsSystem::Instance().Shutdown();
    if (g_failures != 0)
    {
        std::cerr << g_failures << " physics broadcast check(s) failed\n";
        return 1;
    }
    std::cout << "Physics broadcast checks passed\n";
    return 0;
}
