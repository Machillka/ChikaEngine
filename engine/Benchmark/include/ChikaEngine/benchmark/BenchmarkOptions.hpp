#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace ChikaEngine::Benchmark
{
    enum class BenchmarkMode
    {
        Serial,
        Jobs,
        Gpu,
    };

    enum class BenchmarkSceneId
    {
        Empty,
        Instance1K,
        Instance10K,
        Instance50K,
        Instance100K,
        StateDiversity,
        Dynamic5K,
    };

    struct BenchmarkOptions
    {
        BenchmarkSceneId scene = BenchmarkSceneId::Instance1K;
        BenchmarkMode mode = BenchmarkMode::Serial;
        uint32_t workers = 1;
        uint32_t warmupFrames = 300;
        uint32_t sampleFrames = 1000;
        uint32_t flushFrames = 3;
        uint32_t width = 1280;
        uint32_t height = 720;
        uint32_t seed = 0xC41CAu;
        uint32_t sceneBuildTimeoutMs = 120'000;
        std::filesystem::path output = "benchmark-results/run.json";
        bool showHelp = false;
    };

    struct BenchmarkOptionsParseResult
    {
        bool success = false;
        BenchmarkOptions options;
        std::string error;
    };

    /** @brief Parses the strict benchmark CLI so invalid runs fail before engine startup. */
    BenchmarkOptionsParseResult ParseBenchmarkOptions(int argc, const char* const* argv);
    /** @brief Returns the stable CLI help text used by both users and tests. */
    std::string BenchmarkUsage();
    /** @brief Converts a benchmark mode to its schema spelling. */
    std::string_view ToString(BenchmarkMode mode);
    /** @brief Converts a deterministic scene id to its CLI and schema spelling. */
    std::string_view ToString(BenchmarkSceneId scene);
} // namespace ChikaEngine::Benchmark
