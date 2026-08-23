#include "ChikaEngine/base/ObjectPool.h"
#include <iostream>
#include <stdexcept>
#include <vector>

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

    struct TrackedObject
    {
        explicit TrackedObject(int initialValue = 0) : value(initialValue)
        {
            ++liveCount;
        }

        TrackedObject(const TrackedObject&) = delete;
        TrackedObject& operator=(const TrackedObject&) = delete;
        TrackedObject(TrackedObject&&) = delete;
        TrackedObject& operator=(TrackedObject&&) = delete;

        ~TrackedObject()
        {
            --liveCount;
        }

        int value = 0;
        static inline int liveCount = 0;
    };

    struct ThrowingObject
    {
        explicit ThrowingObject(bool shouldThrow)
        {
            if (shouldThrow)
                throw std::runtime_error("expected construction failure");
        }
    };

    void TestInitialCapacityAndZeroCapacityGrowth()
    {
        ChikaEngine::Core::ObjectPool<TrackedObject> initialized(3);
        Check(initialized.Capacity() == 3 && initialized.AvailableCount() == 3 && initialized.InUseCount() == 0, "constructor establishes requested capacity");

        ChikaEngine::Core::ObjectPool<TrackedObject> empty(0);
        TrackedObject* object = empty.Get();
        Check(object != nullptr, "zero-capacity pool grows on first acquisition");
        Check(empty.Capacity() == 1 && empty.InUseCount() == 1 && empty.AvailableCount() == 0, "first acquisition updates pool counts");
        Check(empty.Release(object), "zero-capacity pool releases its first object");
    }

    void TestStableAddressesAcrossGrowth()
    {
        ChikaEngine::Core::ObjectPool<TrackedObject> pool(1);
        TrackedObject* first = pool.Emplace(42);
        std::vector<TrackedObject*> acquired{ first };

        for (int value = 0; value < 64; ++value)
            acquired.push_back(pool.Emplace(value));

        Check(first->value == 42, "growth preserves the first object's address and value");
        Check(pool.Capacity() >= acquired.size() && pool.InUseCount() == acquired.size(), "growth provides enough live slots");
        for (TrackedObject* object : acquired)
            Check(pool.Release(object), "every grown slot can be released");
        Check(pool.InUseCount() == 0 && pool.AvailableCount() == pool.Capacity(), "all grown slots return to the free list");
    }

    void TestReleaseValidationAndReuse()
    {
        ChikaEngine::Core::ObjectPool<TrackedObject> pool(1);
        TrackedObject foreign(7);
        Check(!pool.Release(nullptr), "null release is rejected");
        Check(!pool.Release(&foreign), "foreign pointer release is rejected");

        TrackedObject* first = pool.Emplace(9);
        Check(pool.IsAcquired(first), "acquired object is reported active");
        Check(pool.Release(first), "owned object release succeeds");
        Check(!pool.IsAcquired(first), "released object is no longer active");
        Check(!pool.Release(first), "duplicate release is rejected");

        TrackedObject* reused = pool.Get();
        Check(reused == first, "released slot is reused without allocating another slot");
        Check(reused->value == 0, "reused slot contains a newly default-constructed object");
        Check(pool.Release(reused), "reused object can be released");
    }

    void TestClearAndPoolDestruction()
    {
        const int baselineLiveCount = TrackedObject::liveCount;
        {
            ChikaEngine::Core::ObjectPool<TrackedObject> pool(2);
            TrackedObject* first = pool.Emplace(1);
            TrackedObject* second = pool.Emplace(2);
            Check(first != nullptr && second != nullptr, "pool acquires objects before clear");
            Check(TrackedObject::liveCount == baselineLiveCount + 2, "acquisition constructs active objects");

            pool.Clear();
            Check(TrackedObject::liveCount == baselineLiveCount, "clear destroys every active object");
            Check(pool.InUseCount() == 0 && pool.AvailableCount() == pool.Capacity(), "clear retains reusable capacity");

            TrackedObject* afterClear = pool.Emplace(3);
            Check(afterClear != nullptr, "pool acquires an object after clear");
            Check(TrackedObject::liveCount == baselineLiveCount + 1, "pool remains usable after clear");
        }
        Check(TrackedObject::liveCount == baselineLiveCount, "pool destruction destroys outstanding objects");
    }

    void TestConstructorFailureReturnsSlot()
    {
        ChikaEngine::Core::ObjectPool<ThrowingObject> pool(1);
        try
        {
            [[maybe_unused]] ThrowingObject* unexpected = pool.Emplace(true);
            Check(false, "throwing constructor propagates failure");
        }
        catch (const std::runtime_error&)
        {
        }

        Check(pool.InUseCount() == 0 && pool.AvailableCount() == 1, "failed construction returns the slot to the pool");
        ThrowingObject* object = pool.Emplace(false);
        Check(object != nullptr && pool.Release(object), "pool remains usable after construction failure");
    }
} // namespace

int main()
{
    TestInitialCapacityAndZeroCapacityGrowth();
    TestStableAddressesAcrossGrowth();
    TestReleaseValidationAndReuse();
    TestClearAndPoolDestruction();
    TestConstructorFailureReturnsSlot();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " object pool test(s) failed\n";
        return 1;
    }

    std::cout << "Object pool tests passed\n";
    return 0;
}
