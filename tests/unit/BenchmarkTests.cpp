#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/benchmark/BenchmarkOptions.hpp"
#include "ChikaEngine/benchmark/BenchmarkResult.hpp"
#include "ChikaEngine/benchmark/BenchmarkRunner.hpp"
#include "ChikaEngine/benchmark/BenchmarkSceneFactory.hpp"
#include "ChikaEngine/benchmark/BenchmarkSceneLayout.hpp"
#include "ChikaEngine/benchmark/SampleStatistics.hpp"
#include "ChikaEngine/scene/scene.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
    /** @brief Reports one focused contract failure without introducing a test framework dependency. */
    int Fail(const char* message)
    {
        std::cerr << "FAILED: " << message << '\n';
        return 1;
    }
} // namespace

/** @brief Verifies CLI, deterministic scenes, lifecycle accounting and schema round trips. */
int main()
{
    namespace Benchmark = ChikaEngine::Benchmark;

    const char* validArguments[] = { "ChikaBenchmark", "--scene", "dynamic-5k", "--mode", "serial", "--workers", "1", "--warmup", "2", "--frames", "3", "--output", "test.json" };
    const auto validOptions = Benchmark::ParseBenchmarkOptions(static_cast<int>(std::size(validArguments)), validArguments);
    if (!validOptions.success || validOptions.options.scene != Benchmark::BenchmarkSceneId::Dynamic5K || validOptions.options.sampleFrames != 3)
        return Fail("valid benchmark CLI was rejected");

    const char* invalidArguments[] = { "ChikaBenchmark", "--scene", "typo" };
    if (Benchmark::ParseBenchmarkOptions(static_cast<int>(std::size(invalidArguments)), invalidArguments).success)
        return Fail("invalid benchmark scene was accepted");

    const std::vector<double> timings{ 5.0, 1.0, 4.0, 2.0, 3.0 };
    const auto distribution = Benchmark::ComputeSampleDistribution(timings);
    if (distribution.count != 5 || distribution.minimum != 1.0 || distribution.maximum != 5.0 || distribution.mean != 3.0 || distribution.p50 != 3.0 || distribution.p95 != 5.0 || distribution.p99 != 5.0)
        return Fail("sample distribution contract changed");
    if (Benchmark::ComputeSampleDistribution(std::span<const double>{}).count != 0)
        return Fail("empty sample distribution is not empty");
    bool invalidTimingRejected = false;
    try
    {
        const std::vector<double> invalidTimings{ 1.0, -0.5 };
        (void)Benchmark::ComputeSampleDistribution(invalidTimings);
    }
    catch (const std::invalid_argument&)
    {
        invalidTimingRejected = true;
    }
    if (!invalidTimingRejected)
        return Fail("negative benchmark timing was accepted");

    const auto layoutA = Benchmark::GenerateBenchmarkSceneLayout(Benchmark::BenchmarkSceneId::Instance1K, 42);
    const auto layoutB = Benchmark::GenerateBenchmarkSceneLayout(Benchmark::BenchmarkSceneId::Instance1K, 42);
    const auto layoutC = Benchmark::GenerateBenchmarkSceneLayout(Benchmark::BenchmarkSceneId::Instance1K, 43);
    if (layoutA.objects.size() != 1'000 || layoutA.stableHash == 0 || layoutA.stableHash != layoutB.stableHash || layoutA.stableHash == layoutC.stableHash)
        return Fail("deterministic scene layout or hash is incorrect");

    const auto diverse = Benchmark::GenerateBenchmarkSceneLayout(Benchmark::BenchmarkSceneId::StateDiversity, 42);
    if (diverse.objects.size() != 10'000 || diverse.objects[0].meshVariant == diverse.objects[1].meshVariant)
        return Fail("state-diversity scene does not vary resource identity");

    const auto dynamic = Benchmark::GenerateBenchmarkSceneLayout(Benchmark::BenchmarkSceneId::Dynamic5K, 42);
    if (dynamic.objects.size() != 5'000 || Benchmark::ComputeDynamicPosition(dynamic.objects.front(), 0) == Benchmark::ComputeDynamicPosition(dynamic.objects.front(), 60))
        return Fail("dynamic scene does not produce deterministic motion");

    Benchmark::BenchmarkResult initialResult;
    initialResult.configuration.warmupFrames = 2;
    initialResult.configuration.sampleFrames = 3;
    initialResult.configuration.flushFrames = 2;
    Benchmark::BenchmarkRunner runner(std::move(initialResult));
    for (uint64_t frame = 0; frame < 7; ++frame)
    {
        std::optional<Benchmark::BenchmarkFrameObservation::CompletedGpuFrame> completedGpu;
        if (frame >= 2)
            completedGpu = Benchmark::BenchmarkFrameObservation::CompletedGpuFrame{ .frameIndex = frame - 2, .gpuTimeMs = static_cast<double>(frame - 2) + 0.25 };
        runner.ObserveFrame({
            .frameIndex = frame,
            .frameTimeMs = static_cast<double>(frame + 1),
            .engineTickCpuTimeMs = 1.0,
            .renderGraphCpuTimeMs = 0.5,
            .completedGpuFrame = completedGpu,
        });
    }
    if (!runner.IsComplete() || runner.GetResult().samples.size() != 3 || runner.GetResult().samples.front().frameIndex != 2 || runner.GetResult().samples.back().frameIndex != 4 || runner.GetResult().aggregates.missingGpuSamples != 0 || runner.GetResult().samples.front().gpuTimeMs != 2.25 || runner.GetResult().samples.back().gpuTimeMs != 4.25)
        return Fail("benchmark runner lifecycle or sample accounting is incorrect");

    const auto root = std::filesystem::temp_directory_path() / "chika_benchmark_tests";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const auto resultPath = root / "roundtrip.json";
    std::string error;
    if (!Benchmark::WriteBenchmarkResult(resultPath, runner.GetResult(), error))
        return Fail("benchmark result write failed");
    Benchmark::BenchmarkResult roundTrip;
    if (!Benchmark::ReadBenchmarkResult(resultPath, roundTrip, error) || roundTrip.schemaVersion != Benchmark::kBenchmarkResultSchemaVersion || roundTrip.samples.size() != 3)
        return Fail("benchmark result schema round trip failed");
    const auto unsupportedPath = root / "unsupported.json";
    {
        std::ofstream unsupported(unsupportedPath);
        unsupported << R"({"schemaVersion":999})";
    }
    if (Benchmark::ReadBenchmarkResult(unsupportedPath, roundTrip, error))
        return Fail("unsupported benchmark schema was accepted");

    ChikaEngine::Asset::AssetManager missingAssets;
    if (!missingAssets.Initialize({ .assetRoot = root / "MissingAssets", .createRoot = true, .scanAssets = true, .createMissingMeta = false, .importAssets = false, .enableHotReload = false }))
        return Fail("empty fixture asset manager failed to initialize");
    ChikaEngine::Framework::Scene scene;
    Benchmark::BenchmarkSceneFactory factory;
    if (factory.Build(scene, missingAssets, Benchmark::BenchmarkSceneId::Instance1K, 42).success)
        return Fail("scene factory accepted missing benchmark fixtures");

    missingAssets.Shutdown();
    std::filesystem::remove_all(root, filesystemError);
    return 0;
}
