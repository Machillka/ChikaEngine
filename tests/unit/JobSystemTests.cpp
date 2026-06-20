#include "ChikaEngine/jobs/Jobs.hpp"
#include "ChikaEngine/profiler/ProfilerName.hpp"
#include "ChikaEngine/profiler/ProfilerSession.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
    using namespace ChikaEngine::Jobs;
    std::atomic<int> g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            g_failures.fetch_add(1, std::memory_order_relaxed);
        }
    }

    /** @brief Verifies worker startup, exact-once execution, restart, and startup rollback. */
    void TestLifecycleAndSimpleExecution()
    {
        for (uint32_t workerCount : { 1u, 2u, 4u })
        {
            JobSystem jobs;
            Check(jobs.Initialize({ .workerCount = workerCount, .jobCapacity = 2'048 }), "job system must initialize requested workers");
            std::atomic<uint32_t> executions = 0;
            std::vector<JobHandle> handles;
            for (uint32_t index = 0; index < 1'000; ++index)
                handles.push_back(jobs.Schedule("Test.Simple", [&] { executions.fetch_add(1, std::memory_order_relaxed); }));
            for (JobHandle handle : handles)
            {
                jobs.Wait(handle);
                Check(jobs.Release(handle), "completed simple job must release");
            }
            Check(executions.load() == 1'000, "every simple job must execute exactly once");
            Check(jobs.GetStatistics().completedJobs == 1'000, "completed statistic must match executed jobs");
            jobs.Shutdown();
            Check(!jobs.IsInitialized(), "shutdown must join every worker");
        }

        JobSystem failed;
        Check(!failed.Initialize({ .workerCount = 4, .failWorkerStartAt = 1 }), "injected worker startup failure must roll back initialization");
        failed.Shutdown();
    }

    /** @brief Verifies bounded storage, stale generation rejection, and exception transport. */
    void TestGenerationAndExceptions()
    {
        JobSystem jobs;
        Check(jobs.Initialize({ .workerCount = 1, .jobCapacity = 1 }), "single-slot scheduler must initialize");
        const JobHandle first = jobs.Schedule("Test.First", [] {});
        const JobHandle exhausted = jobs.Schedule("Test.Exhausted", [] {});
        Check(!exhausted.IsValid(), "full job storage must return an invalid handle");
        jobs.Wait(first);
        Check(jobs.Release(first), "first generation must release");
        const JobHandle second = jobs.Schedule("Test.Second", [] {});
        Check(second.index == first.index && second.generation != first.generation, "recycled slot must advance generation");
        bool rejectedStale = false;
        try
        {
            jobs.Wait(first);
        }
        catch (const InvalidJobHandleError&)
        {
            rejectedStale = true;
        }
        Check(rejectedStale, "stale generation handle must be rejected");
        jobs.Wait(second);
        jobs.Release(second);
        jobs.Shutdown();

        Check(jobs.Initialize({ .workerCount = 2 }), "scheduler must support repeated initialize after shutdown");
        const JobHandle failed = jobs.Schedule("Test.Throw", [] { throw std::runtime_error("job failure"); });
        bool propagated = false;
        try
        {
            jobs.Wait(failed);
        }
        catch (const std::runtime_error& exception)
        {
            propagated = std::string_view(exception.what()) == "job failure";
        }
        Check(propagated && jobs.GetState(failed) == JobState::Failed, "job exception must be stored and rethrown by Wait");
        jobs.Release(failed);
        jobs.Shutdown();
    }

    /** @brief Verifies DAG ordering, failure policies, and worker-side Parent/Child waiting. */
    void TestDependenciesAndParentChild()
    {
        JobSystem jobs;
        Check(jobs.Initialize({ .workerCount = 4, .jobCapacity = 8'192 }), "dependency scheduler must initialize");
        std::atomic<uint32_t> stage = 0;
        const JobHandle first = jobs.Schedule("Test.Chain.A", [&] { stage.store(1, std::memory_order_release); });
        const JobHandle second = jobs.ScheduleAfter(std::span(&first, 1),
                                                    "Test.Chain.B",
                                                    [&]
                                                    {
                                                        Check(stage.load(std::memory_order_acquire) == 1, "chain dependency must run first");
                                                        stage.store(2);
                                                    });
        const JobHandle third = jobs.ScheduleAfter(std::span(&second, 1),
                                                   "Test.Chain.C",
                                                   [&]
                                                   {
                                                       Check(stage.load() == 2, "chain continuation order must hold");
                                                       stage.store(3);
                                                   });
        jobs.Wait(third);
        Check(stage.load() == 3, "chain must reach final stage");

        std::atomic<uint32_t> fanInCount = 0;
        std::vector<JobHandle> fanIn;
        fanIn.reserve(1'000);
        for (uint32_t index = 0; index < 1'000; ++index)
            fanIn.push_back(jobs.Schedule("Test.FanIn.Source", [&] { fanInCount.fetch_add(1, std::memory_order_relaxed); }));
        const JobHandle join = jobs.ScheduleAfter(fanIn, "Test.FanIn.Join", [&] { Check(fanInCount.load() == 1'000, "fan-in join must wait for every source"); });
        jobs.Wait(join);

        const JobHandle failure = jobs.Schedule("Test.DependencyFailure", [] { throw std::runtime_error("dependency failure"); });
        const JobHandle propagated = jobs.ScheduleAfter(std::span(&failure, 1), "Test.Propagate", [] {}, JobFailurePolicy::Propagate);
        bool propagatedFailure = false;
        try
        {
            jobs.Wait(propagated);
        }
        catch (const std::runtime_error& exception)
        {
            propagatedFailure = std::string_view(exception.what()) == "dependency failure";
        }
        Check(propagatedFailure, "propagate policy must preserve dependency exception");

        std::atomic<uint32_t> diamondExecutions = 0;
        const JobHandle diamondRoot = jobs.Schedule("Test.Diamond.Root", [] {});
        const JobHandle diamondLeft = jobs.ScheduleAfter(std::span(&diamondRoot, 1), "Test.Diamond.Left", [&] { diamondExecutions.fetch_add(1); });
        const JobHandle diamondRight = jobs.ScheduleAfter(std::span(&diamondRoot, 1), "Test.Diamond.Right", [&] { diamondExecutions.fetch_add(1); });
        const JobHandle diamondDependencies[] = { diamondLeft, diamondRight };
        const JobHandle diamondJoin = jobs.ScheduleAfter(diamondDependencies, "Test.Diamond.Join", [&] { diamondExecutions.fetch_add(1); });
        jobs.Wait(diamondJoin);
        Check(diamondExecutions.load() == 3, "diamond continuations must each run exactly once");

        std::atomic<bool> ranAfterFailure = false;
        const JobHandle runAnyway = jobs.ScheduleAfter(std::span(&failure, 1), "Test.RunAnyway", [&] { ranAfterFailure.store(true); }, JobFailurePolicy::RunAnyway);
        jobs.Wait(runAnyway);
        Check(ranAfterFailure.load(), "RunAnyway continuation must execute after a failed dependency");

        const JobHandle cancelled = jobs.ScheduleAfter(std::span(&failure, 1), "Test.Cancelled", [] {}, JobFailurePolicy::Cancel);
        bool cancellationPropagated = false;
        try
        {
            jobs.Wait(cancelled);
        }
        catch (const JobCancelledError&)
        {
            cancellationPropagated = true;
        }
        Check(cancellationPropagated, "Cancel policy must expose dependency cancellation to Wait");

        for (JobHandle handle : fanIn)
            jobs.Release(handle);
        for (JobHandle handle : { first, second, third, join, failure, propagated, diamondRoot, diamondLeft, diamondRight, diamondJoin, runAnyway, cancelled })
            jobs.Release(handle);
        jobs.Shutdown();

        Check(jobs.Initialize({ .workerCount = 1 }), "single-worker parent test must initialize");
        const JobHandle gate = jobs.Schedule("Test.ParentGate", [] {}, JobTarget::MainThread);
        JobHandle parent;
        std::atomic<uint32_t> childExecutions = 0;
        std::atomic<bool> parentStarted = false;
        parent = jobs.ScheduleAfter(std::span(&gate, 1),
                                    "Test.Parent",
                                    [&]
                                    {
                                        parentStarted.store(true, std::memory_order_release);
                                        const JobHandle child = jobs.ScheduleChild(parent, "Test.Child", [&] { childExecutions.fetch_add(1); });
                                        jobs.Wait(child);
                                        jobs.Release(child);
                                    });
        jobs.PumpMainThreadJobs();
        while (!parentStarted.load(std::memory_order_acquire))
            std::this_thread::yield();
        jobs.Wait(parent);
        Check(childExecutions.load() == 1, "worker-side wait-help must execute a child with one worker");
        jobs.Release(gate);
        jobs.Release(parent);
        jobs.Shutdown();
    }

    /** @brief Runs the same chunked output at a fixed worker count and returns an order-sensitive hash. */
    uint64_t BuildDeterministicParallelHash(uint32_t workerCount)
    {
        JobSystem jobs;
        jobs.Initialize({ .workerCount = workerCount, .jobCapacity = 4'096, .parallelForMinItems = 1 });
        constexpr uint32_t count = 10'003;
        constexpr uint32_t grain = 127;
        const uint32_t chunkCount = (count + grain - 1u) / grain;
        std::vector<std::vector<uint32_t>> chunks(chunkCount);
        const JobHandle parallel = ParallelFor(jobs,
                                               count,
                                               grain,
                                               "Test.Deterministic",
                                               [&](ParallelForRange range)
                                               {
                                                   auto& output = chunks[range.chunkIndex];
                                                   output.reserve(range.end - range.begin);
                                                   for (uint32_t index = range.begin; index < range.end; ++index)
                                                       output.push_back(index * 17u + 3u);
                                               });
        jobs.Wait(parallel);
        jobs.Release(parallel);
        const std::vector<uint32_t> merged = MergeDeterministicChunks(chunks);
        uint64_t hash = 1'469'598'103'934'665'603ull;
        for (uint32_t value : merged)
            hash = (hash ^ value) * 1'099'511'628'211ull;
        Check(merged.size() == count, "deterministic merge must preserve every chunk output");
        jobs.Shutdown();
        return hash;
    }

    /** @brief Verifies range coverage, deterministic merge, nested waits, and observable stealing. */
    void TestParallelForAndStealing()
    {
        JobSystem jobs;
        Check(jobs.Initialize({ .workerCount = 4, .jobCapacity = 16'384, .parallelForMinItems = 1 }), "parallel scheduler must initialize");
        constexpr uint32_t count = 10'003;
        std::vector<std::atomic<uint32_t>> visits(count);
        const JobHandle parallel = ParallelFor(jobs,
                                               count,
                                               127,
                                               "Test.ParallelFor",
                                               [&](ParallelForRange range)
                                               {
                                                   for (uint32_t index = range.begin; index < range.end; ++index)
                                                       visits[index].fetch_add(1, std::memory_order_relaxed);
                                               });
        jobs.Wait(parallel);
        for (const auto& visit : visits)
            Check(visit.load() == 1, "ParallelFor must visit every index exactly once");
        jobs.Release(parallel);

        for (uint32_t countEdge : { 0u, 1u, 7u })
        {
            std::atomic<uint32_t> edgeVisits = 0;
            const JobHandle edge = ParallelFor(jobs, countEdge, 64, "Test.ParallelEdge", [&](ParallelForRange range) { edgeVisits.fetch_add(range.end - range.begin); });
            jobs.Wait(edge);
            jobs.Release(edge);
            Check(edgeVisits.load() == countEdge, "ParallelFor edge ranges must process their exact count");
        }

        const JobHandle throwing = ParallelFor(jobs,
                                               1'024,
                                               64,
                                               "Test.ParallelThrow",
                                               [](ParallelForRange range)
                                               {
                                                   if (range.begin <= 512 && range.end > 512)
                                                       throw std::runtime_error("parallel failure");
                                               });
        bool parallelFailure = false;
        try
        {
            jobs.Wait(throwing);
        }
        catch (const std::runtime_error& exception)
        {
            parallelFailure = std::string_view(exception.what()) == "parallel failure";
        }
        Check(parallelFailure, "ParallelFor join must propagate chunk exceptions");
        jobs.Release(throwing);

        JobHandle gate = jobs.Schedule("Test.StealGate", [] {}, JobTarget::MainThread);
        JobHandle root;
        std::atomic<bool> rootStarted = false;
        root = jobs.ScheduleAfter(std::span(&gate, 1),
                                  "Test.StealRoot",
                                  [&]
                                  {
                                      rootStarted.store(true, std::memory_order_release);
                                      for (uint32_t index = 0; index < 2'000; ++index)
                                      {
                                          JobHandle child = jobs.ScheduleChild(root, "Test.StealChild", [] { std::this_thread::yield(); });
                                          jobs.Detach(child);
                                      }
                                  });
        jobs.PumpMainThreadJobs();
        while (!rootStarted.load(std::memory_order_acquire))
            std::this_thread::yield();
        jobs.Wait(root);
        Check(jobs.GetStatistics().successfulSteals > 0, "worker-local fan-out must produce observable stealing");
        jobs.Release(gate);
        jobs.Release(root);
        jobs.Shutdown();

        const uint64_t expectedHash = BuildDeterministicParallelHash(1);
        for (uint32_t workerCount : { 2u, 4u, 8u })
            Check(BuildDeterministicParallelHash(workerCount) == expectedHash, "deterministic merge hash must not depend on worker count");

        JobSystem nestedJobs;
        nestedJobs.Initialize({ .workerCount = 1, .parallelForMinItems = 1 });
        std::atomic<uint32_t> nestedCount = 0;
        const JobHandle nested = nestedJobs.Schedule("Test.NestedParallel",
                                                     [&]
                                                     {
                                                         const JobHandle child = ParallelFor(nestedJobs, 257, 32, "Test.NestedParallel.Child", [&](ParallelForRange range) { nestedCount.fetch_add(range.end - range.begin); });
                                                         nestedJobs.Wait(child);
                                                         nestedJobs.Release(child);
                                                     });
        nestedJobs.Wait(nested);
        Check(nestedCount.load() == 257, "ParallelFor called inside a worker must complete without deadlock");
        nestedJobs.Release(nested);
        nestedJobs.Shutdown();
    }

    /** @brief Verifies cancel wakeup, drain completion, and submission rejection during shutdown. */
    void TestShutdownCancellation()
    {
        JobSystem jobs;
        Check(jobs.Initialize({ .workerCount = 1, .jobCapacity = 256 }), "cancellation scheduler must initialize");
        std::atomic<bool> releaseBlocker = false;
        std::atomic<bool> blockerStarted = false;
        const JobHandle blocker = jobs.Schedule("Test.Blocker",
                                                [&]
                                                {
                                                    blockerStarted.store(true, std::memory_order_release);
                                                    while (!releaseBlocker.load(std::memory_order_acquire))
                                                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                                });
        while (!blockerStarted.load(std::memory_order_acquire))
            std::this_thread::yield();
        std::atomic<uint32_t> pendingExecutions = 0;
        std::vector<JobHandle> pending;
        for (uint32_t index = 0; index < 100; ++index)
            pending.push_back(jobs.Schedule("Test.CancelPending", [&] { pendingExecutions.fetch_add(1); }));

        std::atomic<bool> waiterCancelled = false;
        std::atomic<bool> waiterStarted = false;
        std::thread waiter(
            [&]
            {
                waiterStarted.store(true, std::memory_order_release);
                try
                {
                    jobs.Wait(pending.front());
                }
                catch (const JobCancelledError&)
                {
                    waiterCancelled.store(true);
                }
            });
        while (!waiterStarted.load(std::memory_order_acquire))
            std::this_thread::yield();
        std::thread unblocker(
            [&]
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                releaseBlocker.store(true, std::memory_order_release);
            });
        jobs.Shutdown(JobShutdownPolicy::CancelPending);
        waiter.join();
        unblocker.join();
        Check(waiterCancelled.load(), "cancel shutdown must wake external waiters with cancellation");
        Check(pendingExecutions.load() == 0, "cancel shutdown must not execute queued callables");
        Check(!jobs.IsAcceptingJobs(), "shutdown must reject new jobs");
        (void)blocker;

        JobSystem drainJobs;
        drainJobs.Initialize({ .workerCount = 4, .jobCapacity = 2'048 });
        std::atomic<uint32_t> drained = 0;
        for (uint32_t index = 0; index < 1'000; ++index)
            drainJobs.Detach(drainJobs.Schedule("Test.Drain", [&] { drained.fetch_add(1, std::memory_order_relaxed); }));
        drainJobs.Shutdown(JobShutdownPolicy::Drain);
        Check(drained.load() == 1'000, "drain shutdown must complete every accepted job");

        JobSystem raceJobs;
        raceJobs.Initialize({ .workerCount = 2, .jobCapacity = 16'384 });
        std::atomic<bool> rejected = false;
        std::thread submitter(
            [&]
            {
                while (true)
                {
                    const JobHandle handle = raceJobs.Schedule("Test.ShutdownRace", [] {});
                    if (!handle.IsValid())
                    {
                        rejected.store(true, std::memory_order_release);
                        break;
                    }
                }
            });
        while (raceJobs.GetStatistics().submittedJobs < 100)
            std::this_thread::yield();
        raceJobs.Shutdown(JobShutdownPolicy::Drain);
        submitter.join();
        Check(rejected.load(std::memory_order_acquire), "submissions racing shutdown must receive a stable rejection");
    }

    /** @brief Verifies worker zones, causal handles, settled counters, and disabled capture behavior. */
    void TestProfilerEvents()
    {
        ChikaEngine::Profiler::ProfilerSession& profiler = ChikaEngine::Profiler::ProfilerSession::Get();
        profiler.Initialize({ .enabled = true, .historyCapacity = 4 });
        JobSystem jobs;
        jobs.Initialize({ .workerCount = 2 });
        profiler.BeginFrame(200);
        const JobHandle handle = jobs.Schedule("Test.ProfiledJob", [] {});
        while (!jobs.IsComplete(handle))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        jobs.Wait(handle);
        profiler.EndFrame(200);
        const auto capture = profiler.GetHistory().Find(200);
        bool foundWorker = false;
        bool foundJobZone = false;
        bool foundCausalPayload = false;
        bool foundIdleCounters = false;
        if (capture)
        {
            for (const auto& thread : capture->threads)
                foundWorker = foundWorker || thread.name.starts_with("Job Worker");
            for (const auto& zone : capture->zones)
                foundJobZone = foundJobZone || ChikaEngine::Profiler::ProfilerNameRegistry::Instance().Resolve(zone.nameId) == "Test.ProfiledJob";
            for (const auto& instant : capture->instants)
                foundCausalPayload = foundCausalPayload || instant.payload == handle.RawValue();
            bool foundQueueEmpty = false;
            bool foundNoActiveWorker = false;
            for (const auto& counter : capture->counters)
            {
                const std::string name = ChikaEngine::Profiler::ProfilerNameRegistry::Instance().Resolve(counter.nameId);
                foundQueueEmpty = foundQueueEmpty || (name == "Jobs.QueueDepth" && counter.value == 0.0);
                foundNoActiveWorker = foundNoActiveWorker || (name == "Jobs.ActiveWorkers" && counter.value == 0.0);
            }
            foundIdleCounters = foundQueueEmpty && foundNoActiveWorker;
        }
        Check(foundWorker && foundJobZone && foundCausalPayload && foundIdleCounters, "job zones, causal IDs, and settled counters must appear on profiler lanes");
        jobs.Release(handle);
        jobs.Shutdown();
        profiler.Shutdown();

        profiler.Initialize({ .enabled = false, .historyCapacity = 2 });
        JobSystem disabledProfilerJobs;
        disabledProfilerJobs.Initialize({ .workerCount = 2 });
        std::atomic<bool> executed = false;
        const JobHandle disabledHandle = disabledProfilerJobs.Schedule("Test.DisabledProfiler", [&] { executed.store(true); });
        disabledProfilerJobs.Wait(disabledHandle);
        Check(executed.load(), "disabled profiler must not change scheduler execution");
        disabledProfilerJobs.Release(disabledHandle);
        disabledProfilerJobs.Shutdown();
        profiler.Shutdown();
    }
} // namespace

int main()
{
    TestLifecycleAndSimpleExecution();
    TestGenerationAndExceptions();
    TestDependenciesAndParentChild();
    TestParallelForAndStealing();
    TestShutdownCancellation();
    TestProfilerEvents();
    return g_failures.load(std::memory_order_relaxed) == 0 ? 0 : 1;
}
