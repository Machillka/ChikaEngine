#include "ChikaEngine/RenderPreparation.hpp"
#include "ChikaEngine/RenderValidation.hpp"
#include "ChikaEngine/jobs/JobSystem.hpp"
#include "ChikaEngine/profiler/ProfilerClock.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using namespace ChikaEngine;
    using Json = nlohmann::json;

    struct Options
    {
        uint32_t warmup = 10;
        uint32_t samples = 30;
        std::filesystem::path output = "docs/dev/results/cpu-renderer/scaling.json";
    };

    struct Fixture
    {
        std::shared_ptr<const Render::RenderWorldSnapshot> snapshot;
        Render::RenderResourceView resources;
        Render::RenderView view;
    };

    /** @brief Parses one bounded unsigned CLI field without locale-dependent conversion. */
    bool ParseUnsigned(std::string_view text, uint32_t& output)
    {
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), output);
        return error == std::errc{} && end == text.data() + text.size();
    }

    /** @brief Keeps the scaling tool contract intentionally smaller than the full engine benchmark CLI. */
    Options ParseOptions(int argc, const char* const* argv)
    {
        Options options;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view option = argv[index];
            if (index + 1 >= argc)
                throw std::invalid_argument("missing value for " + std::string(option));
            const std::string_view value = argv[++index];
            if (option == "--warmup")
            {
                if (!ParseUnsigned(value, options.warmup))
                    throw std::invalid_argument("invalid --warmup");
            }
            else if (option == "--samples")
            {
                if (!ParseUnsigned(value, options.samples) || options.samples == 0)
                    throw std::invalid_argument("invalid --samples");
            }
            else if (option == "--output")
                options.output = value;
            else
                throw std::invalid_argument("unknown option: " + std::string(option));
        }
        return options;
    }

    /** @brief Generates stable scene and resource identities while preserving realistic pass diversity. */
    Fixture BuildFixture(uint32_t objectCount)
    {
        auto snapshot = std::make_shared<Render::RenderWorldSnapshot>();
        snapshot->objects.reserve(objectCount);
        for (uint32_t index = 0; index < objectCount; ++index)
        {
            Render::RenderObjectFlags flags = Render::RenderObjectFlags::Visible | Render::RenderObjectFlags::CastShadow;
            if (index % 20u == 0)
                flags = flags | Render::RenderObjectFlags::Transparent;
            else if (index % 37u == 0)
                flags = flags | Render::RenderObjectFlags::Skinned;

            Render::RenderObjectProxy proxy;
            proxy.mesh = Resource::MeshHandle::FromParts(index % 8u, 1);
            proxy.material = Resource::MaterialHandle::FromParts((index / 3u) % 8u, 1);
            proxy.flags = flags;
            proxy.bounds.center = {
                static_cast<float>(index % 97u) * 0.01f,
                static_cast<float>(index % 53u) * 0.01f,
                -static_cast<float>(index % 211u) * 0.05f,
            };
            snapshot->objects.push_back({
                .handle = Render::RenderObjectHandle::FromParts(index, 1),
                .proxy = std::move(proxy),
            });
        }

        std::vector<Resource::MeshHandle> meshes;
        std::vector<Render::RenderMaterialMetadata> materials;
        for (uint32_t index = 0; index < 8; ++index)
        {
            meshes.push_back(Resource::MeshHandle::FromParts(index, 1));
            materials.push_back({
                .handle = Resource::MaterialHandle::FromParts(index, 1),
                .forwardPipeline = Render::PipelineHandle::FromParts(index * 3u, 1),
                .gbufferPipeline = Render::PipelineHandle::FromParts(index * 3u + 1u, 1),
                .shadowPipeline = Render::PipelineHandle::FromParts(index * 3u + 2u, 1),
            });
        }
        return {
            .snapshot = std::move(snapshot),
            .resources = Render::RenderResourceView(std::move(meshes), std::move(materials)),
            .view = Render::RenderView{ .primary = true },
        };
    }

    /** @brief Uses nearest-rank percentiles so reports remain reproducible for small sample counts. */
    Json BuildDistribution(std::span<const double> samples)
    {
        std::vector<double> sorted(samples.begin(), samples.end());
        std::ranges::sort(sorted);
        const auto percentile = [&sorted](double value)
        {
            const size_t index = static_cast<size_t>(std::ceil(value * static_cast<double>(sorted.size()))) - 1u;
            return sorted[std::min(index, sorted.size() - 1u)];
        };
        double total = 0.0;
        for (double sample : sorted)
            total += sample;
        return {
            { "count", sorted.size() }, { "min", sorted.front() }, { "max", sorted.back() }, { "mean", total / static_cast<double>(sorted.size()) }, { "p50", percentile(0.50) }, { "p95", percentile(0.95) }, { "p99", percentile(0.99) },
        };
    }

    /** @brief Preserves all raw samples beside distributions so future reports can be recomputed. */
    Json BuildMetric(std::string_view unit, const std::vector<double>& samples)
    {
        return {
            { "unit", unit },
            { "distribution", BuildDistribution(samples) },
            { "samples", samples },
        };
    }

    /** @brief Converts correctness hashes without precision loss in JSON consumers that use IEEE doubles. */
    Json HashesToJson(const Render::RenderValidationHashes& hashes)
    {
        return {
            { "visibleSet", std::to_string(hashes.visibleSet) },
            { "packets", std::to_string(hashes.packets) },
            { "batches", std::to_string(hashes.batches) },
            { "drawInput", std::to_string(hashes.drawInput) },
        };
    }

    /** @brief Measures the old snapshot walk against SceneView construction plus the same two-view visibility work. */
    Json RunLayoutComparison(const Fixture& fixture, uint32_t objectCount, const Options& options)
    {
        std::vector<double> snapshotWalk;
        std::vector<double> sceneViewWalk;
        const uint32_t iterationCount = options.warmup + options.samples;
        for (uint32_t iteration = 0; iteration < iterationCount; ++iteration)
        {
            uint64_t begin = Profiler::ProfilerClock::NowNanoseconds();
            const auto legacyMain = Render::BuildVisibility(*fixture.snapshot, fixture.view);
            const auto legacyShadow = Render::BuildVisibility(*fixture.snapshot, fixture.view, true);
            const double legacyMs = static_cast<double>(Profiler::ProfilerClock::NowNanoseconds() - begin) / 1'000'000.0;

            begin = Profiler::ProfilerClock::NowNanoseconds();
            const auto scene = Render::RenderSceneView::Build(fixture.snapshot);
            const auto compactMain = Render::BuildVisibilitySerial(scene, fixture.view);
            const auto compactShadow = Render::BuildVisibilitySerial(scene, fixture.view, true);
            const double compactMs = static_cast<double>(Profiler::ProfilerClock::NowNanoseconds() - begin) / 1'000'000.0;
            if (Render::HashVisibility(legacyMain) != Render::HashVisibility(compactMain) || Render::HashVisibility(legacyShadow) != Render::HashVisibility(compactShadow))
                throw std::runtime_error("layout comparison changed visibility output");
            if (iteration >= options.warmup)
            {
                snapshotWalk.push_back(legacyMs);
                sceneViewWalk.push_back(compactMs);
            }
        }
        return {
            { "objects", objectCount },
            { "legacySnapshotTwoViewVisibility", BuildMetric("ms", snapshotWalk) },
            { "sceneViewBuildAndTwoViewVisibility", BuildMetric("ms", sceneViewWalk) },
        };
    }

    /** @brief Runs one serial or jobs case through the same RenderPreparation entry used by RenderPipeline. */
    Json RunCase(const Fixture& fixture, uint32_t objectCount, uint32_t workers, const Options& options, const Render::RenderValidationHashes& oracle)
    {
        const bool useJobs = workers > 0;
        Jobs::JobSystem jobs;
        if (useJobs && !jobs.Initialize({ .workerCount = workers, .reservedThreads = 0 }))
            throw std::runtime_error("failed to initialize JobSystem");

        std::vector<double> total;
        std::vector<double> sceneView;
        std::vector<double> visibility;
        std::vector<double> packets;
        std::vector<double> sort;
        std::vector<double> queueWait;
        std::vector<double> execution;
        std::vector<double> utilization;
        uint64_t successfulSteals = 0;
        Render::RenderValidationHashes finalHashes;
        const uint32_t iterationCount = options.warmup + options.samples;
        for (uint32_t iteration = 0; iteration < iterationCount; ++iteration)
        {
            const auto before = useJobs ? jobs.GetStatistics() : Jobs::JobSystemStatistics{};
            const auto result = Render::PrepareRenderData(fixture.snapshot,
                                                          fixture.view,
                                                          fixture.view,
                                                          fixture.resources,
                                                          useJobs ? &jobs : nullptr,
                                                          {
                                                              .mode = useJobs ? Render::RenderCpuMode::Jobs : Render::RenderCpuMode::Serial,
                                                              .minimumObjects = 0,
                                                              .visibilityGrainSize = 256,
                                                              .packetGrainSize = 256,
                                                              .sortMinimumPackets = 4'096,
                                                              .sortGrainSize = 1'024,
                                                          });
            finalHashes = result.diagnostics.hashes;
            if (iteration < options.warmup)
                continue;

            const auto after = useJobs ? jobs.GetStatistics() : Jobs::JobSystemStatistics{};
            total.push_back(result.diagnostics.totalCpuTimeMs);
            sceneView.push_back(result.diagnostics.sceneViewCpuTimeMs);
            visibility.push_back(result.diagnostics.visibilityCpuTimeMs);
            packets.push_back(result.diagnostics.packetCpuTimeMs);
            sort.push_back(result.diagnostics.sortCpuTimeMs);
            queueWait.push_back(static_cast<double>(after.queueWaitNanoseconds - before.queueWaitNanoseconds) / 1'000'000.0);
            execution.push_back(static_cast<double>(after.executionNanoseconds - before.executionNanoseconds) / 1'000'000.0);
            utilization.push_back(after.workerUtilization);
            successfulSteals += after.successfulSteals - before.successfulSteals;
        }
        if (useJobs)
            jobs.Shutdown(Jobs::JobShutdownPolicy::Drain);

        return {
            { "objects", objectCount },
            { "mode", useJobs ? "jobs" : "serial" },
            { "workers", workers },
            { "hashes", HashesToJson(finalHashes) },
            { "oracleMatch", finalHashes == oracle },
            { "successfulSteals", successfulSteals },
            { "metrics",
              {
                  { "total", BuildMetric("ms", total) },
                  { "sceneView", BuildMetric("ms", sceneView) },
                  { "visibility", BuildMetric("ms", visibility) },
                  { "packets", BuildMetric("ms", packets) },
                  { "sort", BuildMetric("ms", sort) },
                  { "jobQueueWait", BuildMetric("ms", queueWait) },
                  { "jobExecution", BuildMetric("ms", execution) },
                  { "workerUtilization", BuildMetric("percent", utilization) },
              } },
        };
    }

    /** @brief Writes one complete matrix atomically enough for local benchmark capture. */
    void WriteResult(const std::filesystem::path& output, const Json& result)
    {
        if (!output.parent_path().empty())
            std::filesystem::create_directories(output.parent_path());
        std::ofstream stream(output, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("failed to open benchmark output");
        stream << result.dump(2) << '\n';
        if (!stream)
            throw std::runtime_error("failed to write benchmark output");
    }
} // namespace

/** @brief Captures the complete 1K/10K/50K by serial/1/2/4/8 worker Phase 3 scaling matrix. */
int main(int argc, const char* const* argv)
{
    try
    {
        const Options options = ParseOptions(argc, argv);
        Json cases = Json::array();
        Json layoutComparisons = Json::array();
        for (uint32_t objectCount : std::array{ 1'000u, 10'000u, 50'000u })
        {
            const Fixture fixture = BuildFixture(objectCount);
            layoutComparisons.push_back(RunLayoutComparison(fixture, objectCount, options));
            const auto serial = Render::PrepareRenderData(fixture.snapshot, fixture.view, fixture.view, fixture.resources, nullptr, { .mode = Render::RenderCpuMode::Serial });
            const auto oracle = serial.diagnostics.hashes;
            cases.push_back(RunCase(fixture, objectCount, 0, options, oracle));
            for (uint32_t workers : std::array{ 1u, 2u, 4u, 8u })
                cases.push_back(RunCase(fixture, objectCount, workers, options, oracle));
        }

        const Json result{
            { "schemaVersion", 1 },
            { "hashVersion", Render::kRenderValidationHashVersion },
#ifdef CHIKA_RENDER_CPU_BENCHMARK_BUILD_TYPE
            { "buildType", CHIKA_RENDER_CPU_BENCHMARK_BUILD_TYPE },
#else
            { "buildType", "unknown" },
#endif
            { "warmupIterations", options.warmup },
            { "sampleIterations", options.samples },
            { "layoutComparisons", std::move(layoutComparisons) },
            { "cases", std::move(cases) },
        };
        WriteResult(options.output, result);
        std::cout << "Renderer CPU scaling benchmark written to " << options.output.string() << '\n';
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Renderer CPU benchmark failed: " << exception.what() << '\n';
        return 1;
    }
}
