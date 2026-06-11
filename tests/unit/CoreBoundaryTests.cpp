#include "ChikaEngine/base/FixedStepAccumulator.hpp"
#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/event/EventBus.hpp"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/math/vector3.h"
#include <cmath>
#include <iostream>

namespace
{
    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    bool NearlyEqual(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    struct LifecycleStats
    {
        int awake = 0;
        int start = 0;
        int fixedTick = 0;
        int tick = 0;
        int lateTick = 0;
        int enable = 0;
        int disable = 0;
        int destroy = 0;
    };

    class LifecycleComponent final : public ChikaEngine::Framework::Component
    {
      public:
        explicit LifecycleComponent(LifecycleStats* stats = nullptr) : stats(stats) {}

        void Awake() override
        {
            ++stats->awake;
        }
        void Start() override
        {
            ++stats->start;
        }
        void FixedTick(float) override
        {
            ++stats->fixedTick;
        }
        void Tick(float) override
        {
            ++stats->tick;
        }
        void LateTick(float) override
        {
            ++stats->lateTick;
        }
        void OnEnable() override
        {
            ++stats->enable;
        }
        void OnDisable() override
        {
            ++stats->disable;
        }
        void OnDestroy() override
        {
            ++stats->destroy;
        }

        LifecycleStats* stats = nullptr;
    };

    class SelfRemovingComponent final : public ChikaEngine::Framework::Component
    {
      public:
        explicit SelfRemovingComponent(LifecycleStats* stats) : stats(stats) {}

        void Tick(float) override
        {
            ++stats->tick;
            GetOwner()->RemoveComponent(this);
        }
        void OnDestroy() override
        {
            ++stats->destroy;
        }

        LifecycleStats* stats = nullptr;
    };

    void TestFixedStepAccumulator()
    {
        ChikaEngine::Core::FixedStepAccumulator accumulator(0.1f, 3);
        int callbackCount = 0;

        const uint32_t firstSteps = accumulator.Consume(0.25f,
                                                        [&](float fixedStep)
                                                        {
                                                            Check(NearlyEqual(fixedStep, 0.1f), "fixed step callback value");
                                                            ++callbackCount;
                                                        });
        Check(firstSteps == 2, "fixed step consumes two steps from 0.25 seconds");
        Check(NearlyEqual(accumulator.GetRemainder(), 0.05f), "fixed step preserves remainder");

        const uint32_t clampedSteps = accumulator.Consume(1.0f, [&](float) { ++callbackCount; });
        Check(clampedSteps == 3, "fixed step limits work per frame");
        Check(callbackCount == 5, "fixed step callback count");
    }

    void TestAnimatorTime()
    {
        ChikaEngine::Framework::Animator animator;
        animator.PlaybackSpeed = 2.0f;
        animator.UpdateTime(0.25f, 2.0f);
        Check(NearlyEqual(animator.CurrentTime, 0.5f), "animator applies playback speed once");

        animator.CurrentTime = 1.9f;
        animator.UpdateTime(0.1f, 2.0f);
        Check(NearlyEqual(animator.CurrentTime, 0.1f), "looping animator wraps time");

        animator.IsLoop = false;
        animator.CurrentTime = 1.9f;
        animator.UpdateTime(0.1f, 2.0f);
        Check(NearlyEqual(animator.CurrentTime, 2.0f), "non-looping animator clamps time");
    }

    void TestRigidbodyDefaults()
    {
        ChikaEngine::Framework::Rigidbody rigidbody;
        Check(NearlyEqual(rigidbody.GetColliderRadius(), 0.5f), "rigidbody radius default");
        Check(NearlyEqual(rigidbody.GetColliderHeight(), 1.0f), "rigidbody height default");
        Check(NearlyEqual(rigidbody.GetMass(), 1.0f), "rigidbody mass default");
        Check(NearlyEqual(rigidbody.GetFriction(), 0.5f), "rigidbody friction default");
    }

    void TestCompleteComponentLifecycle()
    {
        LifecycleStats stats;
        ChikaEngine::Framework::GameObject object(1, "Lifecycle");
        auto* component = object.AddComponent<LifecycleComponent>(&stats);

        Check(stats.awake == 1, "component awakes once when attached");
        Check(stats.enable == 1, "component enables when attached to active object");

        object.BeginPlay();
        object.FixedTick(0.02f);
        object.Tick(0.02f);
        object.LateTick(0.02f);
        Check(stats.start == 1, "component starts once when entering play");
        Check(stats.fixedTick == 1 && stats.tick == 1 && stats.lateTick == 1, "component receives complete play update phases");

        component->SetEnabled(false);
        component->SetEnabled(false);
        component->SetEnabled(true);
        Check(stats.disable == 1, "component disables once per state transition");
        Check(stats.enable == 2, "component enables once per state transition");

        object.EndPlay();
        object.BeginPlay();
        Check(stats.start == 2, "component starts once for each play session");

        object.RemoveComponent(component);
        Check(stats.disable == 2, "component disables before removal");
        Check(stats.destroy == 1, "component destroys exactly once when removed");

        LifecycleStats delayedStats;
        ChikaEngine::Framework::GameObject delayedObject(3, "DelayedStart");
        auto* delayedComponent = delayedObject.AddComponent<LifecycleComponent>(&delayedStats);
        delayedComponent->SetEnabled(false);
        delayedObject.BeginPlay();
        Check(delayedStats.start == 0, "disabled component does not start when entering play");
        delayedComponent->SetEnabled(true);
        Check(delayedStats.start == 1, "component starts when first enabled during play");
    }

    void TestDeferredComponentRemoval()
    {
        LifecycleStats stats;
        ChikaEngine::Framework::GameObject object(2, "SelfRemoving");
        object.AddComponent<SelfRemovingComponent>(&stats);
        object.BeginPlay();
        object.Tick(0.016f);

        Check(stats.tick == 1, "self-removing component ticks once");
        Check(stats.destroy == 1, "self-removing component is destroyed after iteration");
        Check(object.GetComponent<SelfRemovingComponent>() == nullptr, "self-removing component no longer belongs to object");
    }

    void TestEventBus()
    {
        struct TestEvent
        {
            int value = 0;
        };

        ChikaEngine::Framework::EventBus events;
        int received = 0;
        const auto subscription = events.Subscribe<TestEvent>([&](const TestEvent& event) { received += event.value; });
        events.Publish(TestEvent{ .value = 3 });
        events.Unsubscribe(subscription);
        events.Publish(TestEvent{ .value = 5 });
        Check(received == 3, "event bus publishes typed events and unsubscribes");
    }

    void TestVector3ComponentMultiply()
    {
        const ChikaEngine::Math::Vector3 result = ChikaEngine::Math::Vector3(2.0f, 3.0f, 4.0f) * ChikaEngine::Math::Vector3(5.0f, 6.0f, 7.0f);
        Check(result == ChikaEngine::Math::Vector3(10.0f, 18.0f, 28.0f), "vector3 component multiply");
    }
} // namespace

int main()
{
    TestFixedStepAccumulator();
    TestAnimatorTime();
    TestRigidbodyDefaults();
    TestCompleteComponentLifecycle();
    TestDeferredComponentRemoval();
    TestEventBus();
    TestVector3ComponentMultiply();

    if (g_failures != 0)
        std::cerr << g_failures << " core boundary test(s) failed\n";
    return g_failures == 0 ? 0 : 1;
}
