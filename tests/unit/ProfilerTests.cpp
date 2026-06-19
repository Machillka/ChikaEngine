#include "ChikaEngine/profiler/ProfilerAggregator.hpp"
#include "ChikaEngine/profiler/ProfilerClock.hpp"
#include "ChikaEngine/profiler/ProfilerEventBuffer.hpp"
#include "ChikaEngine/profiler/ProfilerHistory.hpp"
#include "ChikaEngine/profiler/ProfilerName.hpp"
#include "ChikaEngine/profiler/ProfilerScope.hpp"
#include "ChikaEngine/profiler/ProfilerSession.hpp"
#include "ChikaEngine/profiler/ProfilerThreadRegistry.hpp"
#include "ChikaEngine/profiler/ProfilerTraceExporter.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

bool ProfilerCompileDisabledDoesNotEvaluate();

namespace
{
    using namespace ChikaEngine::Profiler;
    int g_failures = 0;

    /** @brief Records a failed expectation while allowing independent profiler checks to continue. */
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    /** @brief Creates a deterministic event for aggregation tests without reading the real clock. */
    ProfilerEvent MakeZoneEvent(ProfilerEventType type, uint32_t nameId, uint64_t timestampNs)
    {
        return { .timestampNs = timestampNs, .frameId = 7, .threadId = 3, .nameId = nameId, .type = type };
    }

    /** @brief Verifies clock monotonicity and stable name interning under concurrent callers. */
    void TestClockAndNames()
    {
        const uint64_t first = ProfilerClock::NowNanoseconds();
        const uint64_t second = ProfilerClock::NowNanoseconds();
        Check(second >= first, "profiler clock must be monotonic");

        ProfilerNameRegistry& names = ProfilerNameRegistry::Instance();
        const uint32_t expected = names.Intern("Profiler.ConcurrentName");
        std::vector<uint32_t> results(8);
        std::vector<std::thread> threads;
        for (size_t index = 0; index < results.size(); ++index)
            threads.emplace_back([&, index]() { results[index] = names.Intern("Profiler.ConcurrentName"); });
        for (std::thread& thread : threads)
            thread.join();
        for (uint32_t result : results)
            Check(result == expected, "concurrent interning must return one stable ID");
        Check(names.Resolve(expected) == "Profiler.ConcurrentName", "interned names must resolve losslessly");
    }

    /** @brief Verifies full buffers drop new events without overwriting ordered published records. */
    void TestEventBufferOverflow()
    {
        ProfilerEventBuffer buffer(4);
        for (uint64_t index = 0; index < 4; ++index)
            Check(buffer.TryPush({ .timestampNs = index }), "buffer must accept events up to capacity");
        Check(!buffer.TryPush({ .timestampNs = 99 }), "full buffer must reject the newest event");
        Check(buffer.ConsumeDroppedCount() == 1, "overflow must increment the dropped event count");
        std::vector<ProfilerEvent> drained;
        Check(buffer.Drain(drained) == 4, "drain must return every published event");
        for (size_t index = 0; index < drained.size(); ++index)
            Check(drained[index].timestampNs == index, "drain must preserve producer order");
    }

    /** @brief Verifies independent producer buffers preserve 100K ordered events per exiting thread. */
    void TestThreadRegistryConcurrency()
    {
        constexpr uint32_t threadCount = 4;
        constexpr uint32_t eventsPerThread = 100'000;
        ProfilerThreadRegistry registry(eventsPerThread + 1u);
        std::vector<std::thread> threads;
        for (uint32_t worker = 0; worker < threadCount; ++worker)
        {
            threads.emplace_back(
                [&, worker]()
                {
                    registry.SetCurrentThreadName("Profiler Worker " + std::to_string(worker));
                    ProfilerEventBuffer* buffer = registry.GetOrRegisterCurrentThread();
                    const uint32_t threadId = registry.GetCurrentThreadId();
                    for (uint32_t index = 0; index < eventsPerThread; ++index)
                    {
                        const ProfilerEvent event{
                            .timestampNs = index,
                            .payload = worker,
                            .threadId = threadId,
                        };
                        if (!buffer->TryPush(event))
                            return;
                    }
                });
        }
        for (std::thread& thread : threads)
            thread.join();

        const auto batches = registry.DrainAll();
        Check(batches.size() == threadCount, "registry must expose one independent batch per producer thread");
        for (const ProfilerThreadBatch& batch : batches)
        {
            Check(batch.events.size() == eventsPerThread, "each thread buffer must preserve all 100K events");
            Check(batch.droppedEventCount == 0, "configured thread buffers must not overflow");
            Check(batch.threadExited, "thread-local registration must publish thread exit");
            for (size_t index = 0; index < batch.events.size(); ++index)
            {
                Check(batch.events[index].timestampNs == index, "thread buffer event order must remain stable");
                Check(batch.events[index].threadId == batch.threadId, "events must not cross-contaminate thread batches");
            }
        }
    }

    /** @brief Verifies nested zone reconstruction, exclusive time, counters, and malformed recovery. */
    void TestAggregation()
    {
        const uint32_t parent = ProfilerNameRegistry::Instance().Intern("Parent");
        const uint32_t child = ProfilerNameRegistry::Instance().Intern("Child");
        ProfilerThreadBatch batch;
        batch.threadId = 3;
        batch.threadName = "Worker";
        batch.events = {
            MakeZoneEvent(ProfilerEventType::ZoneBegin, parent, 100),
            MakeZoneEvent(ProfilerEventType::ZoneBegin, child, 120),
            MakeZoneEvent(ProfilerEventType::ZoneEnd, child, 150),
            MakeZoneEvent(ProfilerEventType::ZoneEnd, parent, 180),
        };
        ProfilerEvent counter{ .timestampNs = 160, .frameId = 7, .payload = PackProfilerInteger(12), .threadId = 3, .nameId = parent, .type = ProfilerEventType::Counter };
        batch.events.push_back(counter);

        ProfilerAggregator aggregator;
        const auto capture = aggregator.Aggregate(7, 90, 200, { batch }, 100);
        Check(capture->zones.size() == 2, "aggregator must rebuild both nested zones");
        Check(capture->zones[0].inclusiveNs == 80, "parent inclusive duration must match timestamps");
        Check(capture->zones[0].exclusiveNs == 50, "parent exclusive duration must subtract child duration");
        Check(capture->zones[1].depth == 1, "child zone must have nested depth");
        Check(capture->counters.size() == 1 && capture->counters[0].value == 12.0, "integer counter payload must round-trip");
        Check(capture->hitch, "capture above threshold must be marked as a hitch");

        ProfilerThreadBatch malformed;
        malformed.threadId = 3;
        malformed.threadName = "Worker";
        malformed.events = { MakeZoneEvent(ProfilerEventType::ZoneBegin, parent, 100) };
        const auto malformedCapture = aggregator.Aggregate(7, 90, 200, { malformed }, 0);
        Check(malformedCapture->malformedZoneCount == 1, "unclosed zones must be recovered and diagnosed");
    }

    /** @brief Verifies bounded eviction, pin retention, runtime gating, and delayed GPU replacement. */
    void TestHistoryAndSession()
    {
        ProfilerHistory history(2);
        for (uint64_t frameId = 1; frameId <= 2; ++frameId)
        {
            auto frame = std::make_shared<ProfilerFrameCapture>();
            frame->frameId = frameId;
            history.Add(frame);
        }
        Check(history.Pin(1), "existing history frame must be pinnable");
        auto third = std::make_shared<ProfilerFrameCapture>();
        third->frameId = 3;
        history.Add(third);
        Check(history.Snapshot().size() == 2, "history must remain bounded");
        Check(history.Find(1) != nullptr, "pinned frame must survive ring eviction");

        ProfilerSession& session = ProfilerSession::Get();
        session.Initialize({ .enabled = true, .historyCapacity = 4, .threadBufferCapacity = 128, .hitchThresholdNs = 1 });
        session.SetCurrentThreadName("Profiler Test");
        const uint32_t zoneName = ProfilerNameRegistry::Instance().Intern("Session.Zone");
        session.BeginFrame(42);
        {
            ProfilerScope scope(zoneName);
            session.RecordCounter(zoneName, int64_t{ 9 });
            session.RecordCounter(zoneName, 2.5);
        }
        session.EndFrame(42);
        auto capture = session.GetHistory().Find(42);
        Check(capture && !capture->zones.empty(), "RAII scope must publish a completed zone");
        Check(capture && capture->counters.size() == 2 && capture->counters[1].value == 2.5, "integer and floating counters must reach immutable capture");

        const ProfilerGpuTiming timing{ .frameId = 42, .name = "GBuffer", .queueId = 2, .durationNs = 500, .valid = true };
        session.SubmitGpuTimings({ &timing, 1 });
        capture = session.GetHistory().Find(42);
        Check(capture && capture->gpuZones.size() == 1, "delayed GPU result must replace its originating frame capture");
        Check(capture && capture->gpuZones[0].queueId == 2, "GPU queue identity must be preserved");

        session.BeginFrame(43);
        try
        {
            ProfilerScope scope(zoneName);
            throw std::runtime_error("expected profiler scope unwind");
        }
        catch (const std::runtime_error&)
        {
        }
        session.EndFrame(43);
        const auto exceptionCapture = session.GetHistory().Find(43);
        Check(exceptionCapture && exceptionCapture->zones.size() == 1 && !exceptionCapture->zones[0].malformed, "RAII scope must close during exception unwinding");

        session.Initialize({ .enabled = false });
        session.BeginFrame(99);
        session.EndFrame(99);
        Check(session.GetHistory().Snapshot().empty(), "runtime-disabled collection must not publish frames");
        session.Shutdown();
    }

    /** @brief Verifies staged trace export produces parseable Perfetto-compatible event JSON. */
    void TestTraceExport()
    {
        const uint32_t nameId = ProfilerNameRegistry::Instance().Intern("Trace \\\"Quoted\\\"");
        auto capture = std::make_shared<ProfilerFrameCapture>();
        capture->frameId = 10;
        capture->startNs = 1'000;
        capture->endNs = 3'000;
        capture->durationNs = 2'000;
        capture->threads.push_back({ 1, "Main Thread" });
        capture->zones.push_back({ .nameId = nameId, .threadId = 1, .startNs = 1'100, .endNs = 2'100, .inclusiveNs = 1'000, .exclusiveNs = 1'000 });
        capture->gpuZones.push_back({ .nameId = nameId, .queueId = 0, .durationNs = 800 });
        const std::vector<std::shared_ptr<const ProfilerFrameCapture>> captures{ capture };
        const std::filesystem::path output = ".chika/tests/profiler-trace.json";
        std::string error;
        Check(ProfilerTraceExporter::Export(output, captures, { .buildType = "Test" }, &error), "trace export must succeed");
        std::ifstream stream(output);
        const nlohmann::json json = nlohmann::json::parse(stream);
        Check(json["traceEvents"].is_array() && !json["traceEvents"].empty(), "trace export must contain events");
        Check(json["metadata"]["eventSchemaVersion"] == kProfilerEventSchemaVersion, "trace must preserve event schema version");

        const std::filesystem::path invalidOutput = ".chika/tests/profiler-output-directory";
        std::filesystem::create_directories(invalidOutput);
        error.clear();
        Check(!ProfilerTraceExporter::Export(invalidOutput, captures, {}, &error), "trace exporter must report publish failures");
        Check(!std::filesystem::exists(invalidOutput.string() + ".tmp"), "failed trace export must remove its temporary file");
    }
} // namespace

/** @brief Runs focused profiler correctness and contract checks without requiring a window or GPU. */
int main()
{
    TestClockAndNames();
    TestEventBufferOverflow();
    TestThreadRegistryConcurrency();
    TestAggregation();
    TestHistoryAndSession();
    TestTraceExport();
    Check(ProfilerCompileDisabledDoesNotEvaluate(), "compile-disabled macros must erase argument side effects");
    return g_failures == 0 ? 0 : 1;
}
