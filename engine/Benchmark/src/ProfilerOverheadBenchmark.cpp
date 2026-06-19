#include "ChikaEngine/profiler/ProfilerMacros.hpp"
#include "ChikaEngine/profiler/ProfilerSession.hpp"
#include "ChikaEngine/profiler/ProfilerTraceExporter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

void ProfilerCompileDisabledIteration();
void ProfilerBenchmarkWorkKernel();

namespace
{
    volatile double g_profilerBenchmarkSink = 0.0;
#if defined(_WIN32)
    constexpr const char* kBenchmarkPlatform = "Windows";
#elif defined(__APPLE__)
    constexpr const char* kBenchmarkPlatform = "macOS";
#else
    constexpr const char* kBenchmarkPlatform = "Linux";
#endif

    struct Options
    {
        uint32_t iterations = 10'000;
        uint32_t repeats = 9;
        std::filesystem::path output = "docs/dev/results/profiler/overhead.json";
    };

    struct Comparison
    {
        std::vector<double> baselineSamples;
        std::vector<double> candidateSamples;
        std::vector<double> overheadSamples;
        double baselineMedian = 0.0;
        double candidateMedian = 0.0;
        double overheadMedian = 0.0;
    };

    /** @brief Parses the deliberately small benchmark CLI without coupling to engine benchmark scenes. */
    Options ParseOptions(int argc, char** argv)
    {
        Options options;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--iterations" && index + 1 < argc)
                options.iterations = static_cast<uint32_t>(std::stoul(argv[++index]));
            else if (argument == "--repeats" && index + 1 < argc)
                options.repeats = static_cast<uint32_t>(std::stoul(argv[++index]));
            else if (argument == "--output" && index + 1 < argc)
                options.output = argv[++index];
            else
                throw std::invalid_argument("unknown or incomplete argument: " + argument);
        }
        options.iterations = std::max(1u, options.iterations);
        options.repeats = std::max(3u, options.repeats);
        return options;
    }

    /** @brief Returns the median wall time to reduce scheduler noise without hiding raw samples. */
    double Median(std::vector<double> values)
    {
        std::ranges::sort(values);
        return values[values.size() / 2];
    }

    /** @brief Measures repeated calls and returns milliseconds for each independent sample. */
    template <typename Function> std::vector<double> Measure(uint32_t iterations, uint32_t repeats, Function&& function)
    {
        std::vector<double> samples;
        samples.reserve(repeats);
        for (uint32_t repeat = 0; repeat < repeats; ++repeat)
        {
            const auto begin = std::chrono::steady_clock::now();
            for (uint32_t iteration = 0; iteration < iterations; ++iteration)
                function();
            const auto end = std::chrono::steady_clock::now();
            samples.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
        }
        return samples;
    }

    /** @brief Converts measured milliseconds to overhead percentage relative to the uninstrumented path. */
    double OverheadPercent(double measuredMs, double baselineMs)
    {
        return baselineMs <= 0.0 ? 0.0 : (measuredMs / baselineMs - 1.0) * 100.0;
    }

    /** @brief Interleaves baseline and candidate samples so clock and thermal drift affect both paths equally. */
    template <typename BaselineMeasure, typename CandidateMeasure> Comparison Compare(uint32_t repeats, BaselineMeasure&& baselineMeasure, CandidateMeasure&& candidateMeasure)
    {
        Comparison comparison;
        comparison.baselineSamples.reserve(repeats);
        comparison.candidateSamples.reserve(repeats);
        comparison.overheadSamples.reserve(repeats);
        for (uint32_t repeat = 0; repeat < repeats; ++repeat)
        {
            double baseline = 0.0;
            double candidate = 0.0;
            if ((repeat & 1u) == 0)
            {
                baseline = baselineMeasure();
                candidate = candidateMeasure();
            }
            else
            {
                candidate = candidateMeasure();
                baseline = baselineMeasure();
            }
            comparison.baselineSamples.push_back(baseline);
            comparison.candidateSamples.push_back(candidate);
            comparison.overheadSamples.push_back(OverheadPercent(candidate, baseline));
        }
        comparison.baselineMedian = Median(comparison.baselineSamples);
        comparison.candidateMedian = Median(comparison.candidateSamples);
        comparison.overheadMedian = Median(comparison.overheadSamples);
        return comparison;
    }

    /** @brief Executes the normal compiled macro path so runtime gating cost remains observable. */
    void ProfilerRuntimeConfiguredIteration()
    {
        CHIKA_PROFILE_SCOPE("ProfilerBenchmark.Work");
        ProfilerBenchmarkWorkKernel();
    }
} // namespace

/** @brief Supplies enough deterministic arithmetic to measure profiler overhead against useful work. */
void ProfilerBenchmarkWorkKernel()
{
    double value = g_profilerBenchmarkSink + 0.25;
    // A stage-level zone should enclose useful work measured in microseconds, not individual arithmetic operations.
    for (uint32_t index = 1; index <= 16'384; ++index)
        value = value * 1.000000119 + static_cast<double>(index) * 0.000001;
    g_profilerBenchmarkSink = value;
}

/** @brief Runs compile/runtime profiler overhead experiments and persists all raw measurements. */
int main(int argc, char** argv)
{
    try
    {
        const Options options = ParseOptions(argc, argv);
        using ChikaEngine::Profiler::ProfilerSession;
        ProfilerSession& session = ProfilerSession::Get();

        ProfilerBenchmarkWorkKernel();
        const auto measureBaseline = [&]() { return Measure(options.iterations, 1, ProfilerBenchmarkWorkKernel).front(); };
        const auto compileDisabled = Compare(options.repeats, measureBaseline, [&]() { return Measure(options.iterations, 1, ProfilerCompileDisabledIteration).front(); });

        session.Initialize({ .enabled = false, .historyCapacity = 2, .threadBufferCapacity = static_cast<size_t>(options.iterations) * 2u + 16u });
        ProfilerRuntimeConfiguredIteration();
        const auto runtimeDisabled = Compare(options.repeats, measureBaseline, [&]() { return Measure(options.iterations, 1, ProfilerRuntimeConfiguredIteration).front(); });

        session.SetEnabled(true);
        uint64_t profilerFrameId = 0;
        const auto enabled = Compare(options.repeats,
                                     measureBaseline,
                                     [&]()
                                     {
                                         session.BeginFrame(profilerFrameId);
                                         const auto sample = Measure(options.iterations, 1, ProfilerRuntimeConfiguredIteration);
                                         session.EndFrame(profilerFrameId++);
                                         return sample.front();
                                     });
        const auto traceCaptures = session.GetHistory().Snapshot();
        session.Shutdown();

        const std::filesystem::path tracePath = options.output.parent_path() / "sample-trace.json";
        std::string traceError;
        std::vector<std::shared_ptr<const ChikaEngine::Profiler::ProfilerFrameCapture>> traceSample;
        if (!traceCaptures.empty())
        {
            auto sampleCapture = std::make_shared<ChikaEngine::Profiler::ProfilerFrameCapture>(*traceCaptures.back());
            if (sampleCapture->zones.size() > 256)
                sampleCapture->zones.resize(256);
            traceSample.push_back(std::move(sampleCapture));
        }
        const bool traceExported = ChikaEngine::Profiler::ProfilerTraceExporter::Export(tracePath,
                                                                                        traceSample,
                                                                                        {
                                                                                            .applicationName = "ChikaProfilerOverheadBenchmark",
                                                                                            .buildType = CHIKA_PROFILER_BENCHMARK_BUILD_TYPE,
                                                                                            .platform = kBenchmarkPlatform,
                                                                                            .additional = { { "workUnitsPerZone", "16384" }, { "purpose", "Phase 1 profiler trace format sample" }, { "zoneLimit", "256" } },
                                                                                        },
                                                                                        &traceError);

        nlohmann::json output{
            { "schemaVersion", 1 },
            { "buildType", CHIKA_PROFILER_BENCHMARK_BUILD_TYPE },
            { "iterations", options.iterations },
            { "workUnitsPerZone", 16'384 },
            { "repeats", options.repeats },
            { "units", "milliseconds" },
            { "method", "paired-interleaved-median" },
            { "compileDisabled", { { "baselineSamples", compileDisabled.baselineSamples }, { "samples", compileDisabled.candidateSamples }, { "overheadSamples", compileDisabled.overheadSamples }, { "baselineMedian", compileDisabled.baselineMedian }, { "median", compileDisabled.candidateMedian }, { "overheadPercent", compileDisabled.overheadMedian } } },
            { "runtimeDisabled", { { "baselineSamples", runtimeDisabled.baselineSamples }, { "samples", runtimeDisabled.candidateSamples }, { "overheadSamples", runtimeDisabled.overheadSamples }, { "baselineMedian", runtimeDisabled.baselineMedian }, { "median", runtimeDisabled.candidateMedian }, { "overheadPercent", runtimeDisabled.overheadMedian }, { "targetPercent", 0.1 }, { "passed", runtimeDisabled.overheadMedian < 0.1 } } },
            { "enabled", { { "baselineSamples", enabled.baselineSamples }, { "samples", enabled.candidateSamples }, { "overheadSamples", enabled.overheadSamples }, { "baselineMedian", enabled.baselineMedian }, { "median", enabled.candidateMedian }, { "overheadPercent", enabled.overheadMedian }, { "targetPercent", 2.0 }, { "passed", enabled.overheadMedian < 2.0 } } },
            { "trace", { { "path", tracePath.generic_string() }, { "exported", traceExported }, { "error", traceError } } },
            { "sink", g_profilerBenchmarkSink },
        };
        if (const auto parent = options.output.parent_path(); !parent.empty())
            std::filesystem::create_directories(parent);
        std::ofstream stream(options.output, std::ios::binary | std::ios::trunc);
        stream << output.dump(2);
        if (!stream)
            throw std::runtime_error("failed to write profiler overhead result");

        std::cout << "compile-disabled=" << compileDisabled.overheadMedian << "%, runtime-disabled=" << runtimeDisabled.overheadMedian << "%, enabled=" << enabled.overheadMedian << "%\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Profiler overhead benchmark failed: " << exception.what() << '\n';
        return 1;
    }
}
