#include "ChikaEngine/base/FixedStepAccumulator.hpp"
#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/Component.h"
#include "ChikaEngine/component/Rigidbody.hpp"
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

    class LifecycleComponent final : public ChikaEngine::Framework::Component
    {
      public:
        void OnEnable() override
        {
            ++enableCount;
        }

        void OnDisable() override
        {
            ++disableCount;
        }

        int enableCount = 0;
        int disableCount = 0;
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

    void TestComponentEnableLifecycle()
    {
        LifecycleComponent component;
        component.SetEnabled(false);
        component.SetEnabled(false);
        component.SetEnabled(true);

        Check(component.disableCount == 1, "component disables once per state transition");
        Check(component.enableCount == 1, "component enables once per state transition");
    }
} // namespace

int main()
{
    TestFixedStepAccumulator();
    TestAnimatorTime();
    TestRigidbodyDefaults();
    TestComponentEnableLifecycle();

    if (g_failures != 0)
        std::cerr << g_failures << " core boundary test(s) failed\n";
    return g_failures == 0 ? 0 : 1;
}
