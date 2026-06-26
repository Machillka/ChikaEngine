#include "ChikaEngine/benchmark/BenchmarkResult.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <system_error>
#include <thread>

namespace ChikaEngine::Benchmark
{
    namespace
    {
        using Json = nlohmann::json;

        /** @brief Serializes a distribution as an explicit schema object. */
        Json DistributionToJson(const SampleDistribution& value)
        {
            return {
                { "count", value.count }, { "min", value.minimum }, { "max", value.maximum }, { "mean", value.mean }, { "p50", value.p50 }, { "p95", value.p95 }, { "p99", value.p99 },
            };
        }

        /** @brief Deserializes a distribution while allowing future additive schema fields. */
        SampleDistribution DistributionFromJson(const Json& value)
        {
            SampleDistribution result;
            result.count = value.value("count", size_t{});
            result.minimum = value.value("min", 0.0);
            result.maximum = value.value("max", 0.0);
            result.mean = value.value("mean", 0.0);
            result.p50 = value.value("p50", 0.0);
            result.p95 = value.value("p95", 0.0);
            result.p99 = value.value("p99", 0.0);
            return result;
        }

        /** @brief Serializes renderer counters without coupling the RHI module to JSON. */
        Json RenderStatisticsToJson(const Render::RenderFrameStatistics& value)
        {
            return {
                { "passCount", value.passCount },
                { "drawCallCount", value.drawCallCount },
                { "instanceCount", value.instanceCount },
                { "pipelineBindCount", value.pipelineBindCount },
                { "descriptorUpdateCount", value.descriptorUpdateCount },
                { "visibleObjectCount", value.visibleObjectCount },
                { "culledObjectCount", value.culledObjectCount },
                { "packetCount", value.packetCount },
                { "batchCount", value.batchCount },
                { "instancedBatchCount", value.instancedBatchCount },
                { "staticOpaqueObjectCount", value.staticOpaqueObjectCount },
                { "skinnedObjectCount", value.skinnedObjectCount },
                { "transparentObjectCount", value.transparentObjectCount },
                { "invalidResourceObjectCount", value.invalidResourceObjectCount },
                { "preparationFallback", value.preparationFallback },
                { "requestedRenderPath", value.requestedRenderPath },
                { "effectiveRenderPath", value.effectiveRenderPath },
                { "renderPathFallback", value.renderPathFallback },
                { "gpuDrivenInstanceCount", value.gpuDrivenInstanceCount },
                { "gpuDrivenVisibleCount", value.gpuDrivenVisibleCount },
                { "gpuDrivenDrawGroupCount", value.gpuDrivenDrawGroupCount },
                { "gpuDrivenIndirectCommandCount", value.gpuDrivenIndirectCommandCount },
                { "gpuDrivenValidationCompared", value.gpuDrivenValidationCompared },
                { "gpuDrivenValidationMatched", value.gpuDrivenValidationMatched },
                { "gpuDrivenValidationMissingCount", value.gpuDrivenValidationMissingCount },
                { "gpuDrivenValidationExtraCount", value.gpuDrivenValidationExtraCount },
                { "gpuDrivenLayoutHash", value.gpuDrivenLayoutHash },
                { "gpuDrivenVisibilityHash", value.gpuDrivenVisibilityHash },
                { "gpuDrivenIndirectHash", value.gpuDrivenIndirectHash },
                { "validationHashVersion", value.validationHashVersion },
                { "visibleSetHash", value.visibleSetHash },
                { "packetHash", value.packetHash },
                { "batchHash", value.batchHash },
                { "drawInputHash", value.drawInputHash },
                { "preparationCpuTimeMs", value.preparationCpuTimeMs },
                { "sceneViewCpuTimeMs", value.sceneViewCpuTimeMs },
                { "resourceViewCpuTimeMs", value.resourceViewCpuTimeMs },
                { "visibilityCpuTimeMs", value.visibilityCpuTimeMs },
                { "packetCpuTimeMs", value.packetCpuTimeMs },
                { "sortCpuTimeMs", value.sortCpuTimeMs },
                { "renderJobsUsed", value.renderJobsUsed },
            };
        }

        /** @brief Restores renderer counters from persisted samples for report and regression tests. */
        Render::RenderFrameStatistics RenderStatisticsFromJson(const Json& value)
        {
            Render::RenderFrameStatistics result;
            result.passCount = value.value("passCount", 0u);
            result.drawCallCount = value.value("drawCallCount", 0u);
            result.instanceCount = value.value("instanceCount", 0u);
            result.pipelineBindCount = value.value("pipelineBindCount", 0u);
            result.descriptorUpdateCount = value.value("descriptorUpdateCount", 0u);
            result.visibleObjectCount = value.value("visibleObjectCount", 0u);
            result.culledObjectCount = value.value("culledObjectCount", 0u);
            result.packetCount = value.value("packetCount", 0u);
            result.batchCount = value.value("batchCount", 0u);
            result.instancedBatchCount = value.value("instancedBatchCount", 0u);
            result.staticOpaqueObjectCount = value.value("staticOpaqueObjectCount", 0u);
            result.skinnedObjectCount = value.value("skinnedObjectCount", 0u);
            result.transparentObjectCount = value.value("transparentObjectCount", 0u);
            result.invalidResourceObjectCount = value.value("invalidResourceObjectCount", 0u);
            result.preparationFallback = value.value("preparationFallback", 0u);
            result.requestedRenderPath = value.value("requestedRenderPath", 0u);
            result.effectiveRenderPath = value.value("effectiveRenderPath", 0u);
            result.renderPathFallback = value.value("renderPathFallback", 0u);
            result.gpuDrivenInstanceCount = value.value("gpuDrivenInstanceCount", 0u);
            result.gpuDrivenVisibleCount = value.value("gpuDrivenVisibleCount", 0u);
            result.gpuDrivenDrawGroupCount = value.value("gpuDrivenDrawGroupCount", 0u);
            result.gpuDrivenIndirectCommandCount = value.value("gpuDrivenIndirectCommandCount", 0u);
            result.gpuDrivenValidationCompared = value.value("gpuDrivenValidationCompared", 0u);
            result.gpuDrivenValidationMatched = value.value("gpuDrivenValidationMatched", 0u);
            result.gpuDrivenValidationMissingCount = value.value("gpuDrivenValidationMissingCount", 0u);
            result.gpuDrivenValidationExtraCount = value.value("gpuDrivenValidationExtraCount", 0u);
            result.gpuDrivenLayoutHash = value.value("gpuDrivenLayoutHash", uint64_t{});
            result.gpuDrivenVisibilityHash = value.value("gpuDrivenVisibilityHash", uint64_t{});
            result.gpuDrivenIndirectHash = value.value("gpuDrivenIndirectHash", uint64_t{});
            result.validationHashVersion = value.value("validationHashVersion", 0u);
            result.visibleSetHash = value.value("visibleSetHash", uint64_t{});
            result.packetHash = value.value("packetHash", uint64_t{});
            result.batchHash = value.value("batchHash", uint64_t{});
            result.drawInputHash = value.value("drawInputHash", uint64_t{});
            result.preparationCpuTimeMs = value.value("preparationCpuTimeMs", 0.0);
            result.sceneViewCpuTimeMs = value.value("sceneViewCpuTimeMs", 0.0);
            result.resourceViewCpuTimeMs = value.value("resourceViewCpuTimeMs", 0.0);
            result.visibilityCpuTimeMs = value.value("visibilityCpuTimeMs", 0.0);
            result.packetCpuTimeMs = value.value("packetCpuTimeMs", 0.0);
            result.sortCpuTimeMs = value.value("sortCpuTimeMs", 0.0);
            result.renderJobsUsed = value.value("renderJobsUsed", false);
            return result;
        }

        /** @brief Produces the complete versioned JSON document from a result model. */
        Json ResultToJson(const BenchmarkResult& result)
        {
            Json samples = Json::array();
            for (const auto& sample : result.samples)
            {
                samples.push_back({
                    { "frameIndex", sample.frameIndex },
                    { "frameTimeMs", sample.frameTimeMs },
                    { "engineTickCpuTimeMs", sample.engineTickCpuTimeMs },
                    { "renderGraphCpuTimeMs", sample.renderGraphCpuTimeMs },
                    { "gpuTimeMs", sample.gpuTimeMs },
                    { "gpuTimingAvailable", sample.gpuTimingAvailable },
                    { "renderStatistics", RenderStatisticsToJson(sample.renderStatistics) },
                    { "jobQueueWaitNanoseconds", sample.jobQueueWaitNanoseconds },
                    { "jobExecutionNanoseconds", sample.jobExecutionNanoseconds },
                    { "successfulJobSteals", sample.successfulJobSteals },
                    { "workerUtilization", sample.workerUtilization },
                });
            }

            return {
                { "schemaVersion", result.schemaVersion },
                { "build",
                  {
                      { "engineVersion", result.build.engineVersion },
                      { "buildType", result.build.buildType },
                      { "compiler", result.build.compiler },
                  } },
                { "hardware",
                  {
                      { "operatingSystem", result.hardware.operatingSystem },
                      { "logicalCpuCount", result.hardware.logicalCpuCount },
                      { "gpuName", result.hardware.gpuName },
                  } },
                { "configuration",
                  {
                      { "scene", result.configuration.scene },
                      { "requestedMode", result.configuration.requestedMode },
                      { "effectiveMode", result.configuration.effectiveMode },
                      { "workers", result.configuration.workers },
                      { "warmupFrames", result.configuration.warmupFrames },
                      { "sampleFrames", result.configuration.sampleFrames },
                      { "flushFrames", result.configuration.flushFrames },
                      { "width", result.configuration.width },
                      { "height", result.configuration.height },
                      { "seed", result.configuration.seed },
                      { "sceneBuildTimeoutMs", result.configuration.sceneBuildTimeoutMs },
                      { "sceneConfigHash", result.configuration.sceneConfigHash },
                      { "vSync", result.configuration.vSync },
                  } },
                { "samples", std::move(samples) },
                { "aggregates",
                  {
                      { "frameTime", DistributionToJson(result.aggregates.frameTime) },
                      { "engineTickCpuTime", DistributionToJson(result.aggregates.engineTickCpuTime) },
                      { "renderGraphCpuTime", DistributionToJson(result.aggregates.renderGraphCpuTime) },
                      { "gpuTime", DistributionToJson(result.aggregates.gpuTime) },
                      { "renderPreparationCpuTime", DistributionToJson(result.aggregates.renderPreparationCpuTime) },
                      { "visibilityCpuTime", DistributionToJson(result.aggregates.visibilityCpuTime) },
                      { "packetCpuTime", DistributionToJson(result.aggregates.packetCpuTime) },
                      { "sortCpuTime", DistributionToJson(result.aggregates.sortCpuTime) },
                      { "jobQueueWaitTime", DistributionToJson(result.aggregates.jobQueueWaitTime) },
                      { "jobExecutionTime", DistributionToJson(result.aggregates.jobExecutionTime) },
                      { "workerUtilization", DistributionToJson(result.aggregates.workerUtilization) },
                      { "missingGpuSamples", result.aggregates.missingGpuSamples },
                  } },
            };
        }

        /** @brief Parses the supported schema after the caller has validated its version. */
        BenchmarkResult ResultFromJson(const Json& json)
        {
            BenchmarkResult result;
            result.schemaVersion = json.at("schemaVersion").get<uint32_t>();
            const auto& build = json.at("build");
            result.build.engineVersion = build.value("engineVersion", "unknown");
            result.build.buildType = build.value("buildType", "unknown");
            result.build.compiler = build.value("compiler", "unknown");
            const auto& hardware = json.at("hardware");
            result.hardware.operatingSystem = hardware.value("operatingSystem", "unknown");
            result.hardware.logicalCpuCount = hardware.value("logicalCpuCount", 0u);
            result.hardware.gpuName = hardware.value("gpuName", "unknown");

            const auto& configuration = json.at("configuration");
            result.configuration.scene = configuration.at("scene").get<std::string>();
            result.configuration.requestedMode = configuration.at("requestedMode").get<std::string>();
            result.configuration.effectiveMode = configuration.at("effectiveMode").get<std::string>();
            result.configuration.workers = configuration.value("workers", 1u);
            result.configuration.warmupFrames = configuration.value("warmupFrames", 0u);
            result.configuration.sampleFrames = configuration.value("sampleFrames", 0u);
            result.configuration.flushFrames = configuration.value("flushFrames", 0u);
            result.configuration.width = configuration.value("width", 0u);
            result.configuration.height = configuration.value("height", 0u);
            result.configuration.seed = configuration.value("seed", 0u);
            result.configuration.sceneBuildTimeoutMs = configuration.value("sceneBuildTimeoutMs", 0u);
            result.configuration.sceneConfigHash = configuration.value("sceneConfigHash", uint64_t{});
            result.configuration.vSync = configuration.value("vSync", false);

            for (const auto& sample : json.at("samples"))
            {
                result.samples.push_back({
                    .frameIndex = sample.at("frameIndex").get<uint64_t>(),
                    .frameTimeMs = sample.at("frameTimeMs").get<double>(),
                    .engineTickCpuTimeMs = sample.at("engineTickCpuTimeMs").get<double>(),
                    .renderGraphCpuTimeMs = sample.at("renderGraphCpuTimeMs").get<double>(),
                    .gpuTimeMs = sample.value("gpuTimeMs", 0.0),
                    .gpuTimingAvailable = sample.value("gpuTimingAvailable", false),
                    .renderStatistics = RenderStatisticsFromJson(sample.at("renderStatistics")),
                    .jobQueueWaitNanoseconds = sample.value("jobQueueWaitNanoseconds", uint64_t{}),
                    .jobExecutionNanoseconds = sample.value("jobExecutionNanoseconds", uint64_t{}),
                    .successfulJobSteals = sample.value("successfulJobSteals", uint64_t{}),
                    .workerUtilization = sample.value("workerUtilization", 0.0),
                });
            }

            const auto& aggregates = json.at("aggregates");
            result.aggregates.frameTime = DistributionFromJson(aggregates.at("frameTime"));
            result.aggregates.engineTickCpuTime = DistributionFromJson(aggregates.at("engineTickCpuTime"));
            result.aggregates.renderGraphCpuTime = DistributionFromJson(aggregates.at("renderGraphCpuTime"));
            result.aggregates.gpuTime = DistributionFromJson(aggregates.at("gpuTime"));
            if (aggregates.contains("renderPreparationCpuTime"))
                result.aggregates.renderPreparationCpuTime = DistributionFromJson(aggregates.at("renderPreparationCpuTime"));
            if (aggregates.contains("visibilityCpuTime"))
                result.aggregates.visibilityCpuTime = DistributionFromJson(aggregates.at("visibilityCpuTime"));
            if (aggregates.contains("packetCpuTime"))
                result.aggregates.packetCpuTime = DistributionFromJson(aggregates.at("packetCpuTime"));
            if (aggregates.contains("sortCpuTime"))
                result.aggregates.sortCpuTime = DistributionFromJson(aggregates.at("sortCpuTime"));
            if (aggregates.contains("jobQueueWaitTime"))
                result.aggregates.jobQueueWaitTime = DistributionFromJson(aggregates.at("jobQueueWaitTime"));
            if (aggregates.contains("jobExecutionTime"))
                result.aggregates.jobExecutionTime = DistributionFromJson(aggregates.at("jobExecutionTime"));
            if (aggregates.contains("workerUtilization"))
                result.aggregates.workerUtilization = DistributionFromJson(aggregates.at("workerUtilization"));
            result.aggregates.missingGpuSamples = aggregates.value("missingGpuSamples", 0u);
            return result;
        }
    } // namespace

    BenchmarkBuildMetadata CollectBenchmarkBuildMetadata()
    {
        BenchmarkBuildMetadata result;
#ifdef CHIKA_BENCHMARK_ENGINE_VERSION
        result.engineVersion = CHIKA_BENCHMARK_ENGINE_VERSION;
#else
        result.engineVersion = "unknown";
#endif
#ifdef CHIKA_BENCHMARK_BUILD_TYPE
        result.buildType = CHIKA_BENCHMARK_BUILD_TYPE;
#else
        result.buildType = "unknown";
#endif
#if defined(__clang__)
        result.compiler = "Clang " __clang_version__;
#elif defined(_MSC_VER)
        result.compiler = "MSVC " + std::to_string(_MSC_VER);
#elif defined(__GNUC__)
        result.compiler = "GCC " __VERSION__;
#else
        result.compiler = "unknown";
#endif
        return result;
    }

    BenchmarkHardwareMetadata CollectBenchmarkHardwareMetadata()
    {
        BenchmarkHardwareMetadata result;
#if defined(_WIN32)
        result.operatingSystem = "Windows";
#elif defined(__linux__)
        result.operatingSystem = "Linux";
#elif defined(__APPLE__)
        result.operatingSystem = "macOS";
#else
        result.operatingSystem = "unknown";
#endif
        result.logicalCpuCount = std::thread::hardware_concurrency();
        return result;
    }

    void ComputeBenchmarkAggregates(BenchmarkResult& result)
    {
        std::vector<double> frameTimes;
        std::vector<double> engineTickTimes;
        std::vector<double> renderGraphTimes;
        std::vector<double> gpuTimes;
        std::vector<double> preparationTimes;
        std::vector<double> visibilityTimes;
        std::vector<double> packetTimes;
        std::vector<double> sortTimes;
        std::vector<double> jobQueueWaitTimes;
        std::vector<double> jobExecutionTimes;
        std::vector<double> workerUtilizations;
        frameTimes.reserve(result.samples.size());
        engineTickTimes.reserve(result.samples.size());
        renderGraphTimes.reserve(result.samples.size());
        gpuTimes.reserve(result.samples.size());
        preparationTimes.reserve(result.samples.size());
        visibilityTimes.reserve(result.samples.size());
        packetTimes.reserve(result.samples.size());
        sortTimes.reserve(result.samples.size());
        jobQueueWaitTimes.reserve(result.samples.size());
        jobExecutionTimes.reserve(result.samples.size());
        workerUtilizations.reserve(result.samples.size());
        result.aggregates.missingGpuSamples = 0;

        for (const auto& sample : result.samples)
        {
            frameTimes.push_back(sample.frameTimeMs);
            engineTickTimes.push_back(sample.engineTickCpuTimeMs);
            renderGraphTimes.push_back(sample.renderGraphCpuTimeMs);
            preparationTimes.push_back(sample.renderStatistics.preparationCpuTimeMs);
            visibilityTimes.push_back(sample.renderStatistics.visibilityCpuTimeMs);
            packetTimes.push_back(sample.renderStatistics.packetCpuTimeMs);
            sortTimes.push_back(sample.renderStatistics.sortCpuTimeMs);
            jobQueueWaitTimes.push_back(static_cast<double>(sample.jobQueueWaitNanoseconds) / 1'000'000.0);
            jobExecutionTimes.push_back(static_cast<double>(sample.jobExecutionNanoseconds) / 1'000'000.0);
            workerUtilizations.push_back(sample.workerUtilization);
            if (sample.gpuTimingAvailable)
                gpuTimes.push_back(sample.gpuTimeMs);
            else
                ++result.aggregates.missingGpuSamples;
        }
        result.aggregates.frameTime = ComputeSampleDistribution(frameTimes);
        result.aggregates.engineTickCpuTime = ComputeSampleDistribution(engineTickTimes);
        result.aggregates.renderGraphCpuTime = ComputeSampleDistribution(renderGraphTimes);
        result.aggregates.gpuTime = ComputeSampleDistribution(gpuTimes);
        result.aggregates.renderPreparationCpuTime = ComputeSampleDistribution(preparationTimes);
        result.aggregates.visibilityCpuTime = ComputeSampleDistribution(visibilityTimes);
        result.aggregates.packetCpuTime = ComputeSampleDistribution(packetTimes);
        result.aggregates.sortCpuTime = ComputeSampleDistribution(sortTimes);
        result.aggregates.jobQueueWaitTime = ComputeSampleDistribution(jobQueueWaitTimes);
        result.aggregates.jobExecutionTime = ComputeSampleDistribution(jobExecutionTimes);
        result.aggregates.workerUtilization = ComputeSampleDistribution(workerUtilizations);
    }

    bool WriteBenchmarkResult(const std::filesystem::path& path, const BenchmarkResult& result, std::string& error)
    {
        try
        {
            if (path.empty())
                throw std::invalid_argument("Benchmark output path is empty");
            if (!path.parent_path().empty())
                std::filesystem::create_directories(path.parent_path());

            const std::filesystem::path temporaryPath = path.string() + ".tmp";
            {
                std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
                if (!stream)
                    throw std::runtime_error("Failed to open temporary benchmark result");
                stream << ResultToJson(result).dump(2) << '\n';
                if (!stream)
                    throw std::runtime_error("Failed to write temporary benchmark result");
            }
            std::error_code filesystemError;
            std::filesystem::remove(path, filesystemError);
            filesystemError.clear();
            std::filesystem::rename(temporaryPath, path, filesystemError);
            if (filesystemError)
                throw std::runtime_error("Failed to replace benchmark result: " + filesystemError.message());
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }

    bool ReadBenchmarkResult(const std::filesystem::path& path, BenchmarkResult& result, std::string& error)
    {
        try
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                throw std::runtime_error("Failed to open benchmark result: " + path.string());
            const Json json = Json::parse(stream);
            const uint32_t version = json.at("schemaVersion").get<uint32_t>();
            if (version == 0 || version > kBenchmarkResultSchemaVersion)
                throw std::runtime_error("Unsupported benchmark schema version: " + std::to_string(version));
            result = ResultFromJson(json);
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }
} // namespace ChikaEngine::Benchmark
