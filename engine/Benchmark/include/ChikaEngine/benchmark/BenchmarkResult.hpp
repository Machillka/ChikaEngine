#pragma once

#include "ChikaEngine/RenderDiagnostics.hpp"
#include "ChikaEngine/benchmark/BenchmarkOptions.hpp"
#include "ChikaEngine/benchmark/SampleStatistics.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ChikaEngine::Benchmark
{
    inline constexpr uint32_t kBenchmarkResultSchemaVersion = 2;

    struct BenchmarkBuildMetadata
    {
        std::string engineVersion;
        std::string buildType;
        std::string compiler;
    };

    struct BenchmarkHardwareMetadata
    {
        std::string operatingSystem;
        uint32_t logicalCpuCount = 0;
        std::string gpuName;
    };

    struct BenchmarkRunConfiguration
    {
        std::string scene;
        std::string requestedMode;
        std::string effectiveMode;
        uint32_t workers = 1;
        uint32_t warmupFrames = 0;
        uint32_t sampleFrames = 0;
        uint32_t flushFrames = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t seed = 0;
        uint32_t sceneBuildTimeoutMs = 0;
        uint64_t sceneConfigHash = 0;
        bool vSync = false;
    };

    struct BenchmarkFrameSample
    {
        uint64_t frameIndex = 0;
        double frameTimeMs = 0.0;
        double engineTickCpuTimeMs = 0.0;
        double renderGraphCpuTimeMs = 0.0;
        double gpuTimeMs = 0.0;
        bool gpuTimingAvailable = false;
        Render::RenderFrameStatistics renderStatistics;
        uint64_t jobQueueWaitNanoseconds = 0;
        uint64_t jobExecutionNanoseconds = 0;
        uint64_t successfulJobSteals = 0;
        double workerUtilization = 0.0;
    };

    struct BenchmarkAggregates
    {
        SampleDistribution frameTime;
        SampleDistribution engineTickCpuTime;
        SampleDistribution renderGraphCpuTime;
        SampleDistribution gpuTime;
        SampleDistribution renderPreparationCpuTime;
        SampleDistribution visibilityCpuTime;
        SampleDistribution packetCpuTime;
        SampleDistribution sortCpuTime;
        SampleDistribution jobQueueWaitTime;
        SampleDistribution jobExecutionTime;
        SampleDistribution workerUtilization;
        uint32_t missingGpuSamples = 0;
    };

    struct BenchmarkResult
    {
        uint32_t schemaVersion = kBenchmarkResultSchemaVersion;
        BenchmarkBuildMetadata build;
        BenchmarkHardwareMetadata hardware;
        BenchmarkRunConfiguration configuration;
        std::vector<BenchmarkFrameSample> samples;
        BenchmarkAggregates aggregates;
    };

    /** @brief Captures build and host metadata available without platform-specific dependencies. */
    BenchmarkBuildMetadata CollectBenchmarkBuildMetadata();
    /** @brief Captures portable host fields; the active RHI adapter is supplied after renderer startup. */
    BenchmarkHardwareMetadata CollectBenchmarkHardwareMetadata();
    /** @brief Recomputes aggregate distributions from raw frame samples. */
    void ComputeBenchmarkAggregates(BenchmarkResult& result);
    /** @brief Stages versioned JSON in a temporary file before replacing the target. */
    bool WriteBenchmarkResult(const std::filesystem::path& path, const BenchmarkResult& result, std::string& error);
    /** @brief Reads and validates the supported benchmark result schema. */
    bool ReadBenchmarkResult(const std::filesystem::path& path, BenchmarkResult& result, std::string& error);
} // namespace ChikaEngine::Benchmark
