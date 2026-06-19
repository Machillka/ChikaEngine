#include "ChikaEngine/benchmark/BenchmarkOptions.hpp"

#include <charconv>
#include <limits>
#include <sstream>
#include <string_view>

namespace ChikaEngine::Benchmark
{
    namespace
    {
        /** @brief Parses an unsigned CLI value without locale or exception-dependent behavior. */
        bool ParseUnsigned(std::string_view text, uint32_t& value)
        {
            if (text.empty())
                return false;
            uint64_t parsed = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
            if (error != std::errc{} || end != text.data() + text.size() || parsed > std::numeric_limits<uint32_t>::max())
                return false;
            value = static_cast<uint32_t>(parsed);
            return true;
        }

        /** @brief Maps the public scene spelling to a closed enum so typos cannot create incomparable runs. */
        bool ParseScene(std::string_view text, BenchmarkSceneId& scene)
        {
            if (text == "empty")
                scene = BenchmarkSceneId::Empty;
            else if (text == "instance-1k")
                scene = BenchmarkSceneId::Instance1K;
            else if (text == "instance-10k")
                scene = BenchmarkSceneId::Instance10K;
            else if (text == "instance-50k")
                scene = BenchmarkSceneId::Instance50K;
            else if (text == "instance-100k")
                scene = BenchmarkSceneId::Instance100K;
            else if (text == "state-diversity")
                scene = BenchmarkSceneId::StateDiversity;
            else if (text == "dynamic-5k")
                scene = BenchmarkSceneId::Dynamic5K;
            else
                return false;
            return true;
        }

        /** @brief Maps requested execution modes while Phase 0 still reports serial as the only effective mode. */
        bool ParseMode(std::string_view text, BenchmarkMode& mode)
        {
            if (text == "serial")
                mode = BenchmarkMode::Serial;
            else if (text == "jobs")
                mode = BenchmarkMode::Jobs;
            else if (text == "gpu")
                mode = BenchmarkMode::Gpu;
            else
                return false;
            return true;
        }
    } // namespace

    std::string_view ToString(BenchmarkMode mode)
    {
        switch (mode)
        {
        case BenchmarkMode::Serial:
            return "serial";
        case BenchmarkMode::Jobs:
            return "jobs";
        case BenchmarkMode::Gpu:
            return "gpu";
        }
        return "unknown";
    }

    std::string_view ToString(BenchmarkSceneId scene)
    {
        switch (scene)
        {
        case BenchmarkSceneId::Empty:
            return "empty";
        case BenchmarkSceneId::Instance1K:
            return "instance-1k";
        case BenchmarkSceneId::Instance10K:
            return "instance-10k";
        case BenchmarkSceneId::Instance50K:
            return "instance-50k";
        case BenchmarkSceneId::Instance100K:
            return "instance-100k";
        case BenchmarkSceneId::StateDiversity:
            return "state-diversity";
        case BenchmarkSceneId::Dynamic5K:
            return "dynamic-5k";
        }
        return "unknown";
    }

    std::string BenchmarkUsage()
    {
        return "Usage: ChikaBenchmark [options]\n"
               "  --scene <empty|instance-1k|instance-10k|instance-50k|instance-100k|state-diversity|dynamic-5k>\n"
               "  --mode <serial|jobs|gpu>\n"
               "  --workers <count>\n"
               "  --warmup <frames>\n"
               "  --frames <frames>\n"
               "  --flush <frames>\n"
               "  --width <pixels> --height <pixels>\n"
               "  --seed <value>\n"
               "  --scene-timeout-ms <milliseconds>\n"
               "  --output <result.json>\n"
               "  --help\n";
    }

    BenchmarkOptionsParseResult ParseBenchmarkOptions(int argc, const char* const* argv)
    {
        BenchmarkOptionsParseResult result;
        result.success = true;

        for (int index = 1; index < argc; ++index)
        {
            const std::string_view option = argv[index];
            if (option == "--help" || option == "-h")
            {
                result.options.showHelp = true;
                continue;
            }

            if (index + 1 >= argc)
            {
                result.success = false;
                result.error = "Missing value for option: " + std::string(option);
                return result;
            }
            const std::string_view value = argv[++index];

            bool parsed = true;
            if (option == "--scene")
                parsed = ParseScene(value, result.options.scene);
            else if (option == "--mode")
                parsed = ParseMode(value, result.options.mode);
            else if (option == "--workers")
                parsed = ParseUnsigned(value, result.options.workers) && result.options.workers > 0;
            else if (option == "--warmup")
                parsed = ParseUnsigned(value, result.options.warmupFrames);
            else if (option == "--frames")
                parsed = ParseUnsigned(value, result.options.sampleFrames) && result.options.sampleFrames > 0;
            else if (option == "--flush")
                parsed = ParseUnsigned(value, result.options.flushFrames);
            else if (option == "--width")
                parsed = ParseUnsigned(value, result.options.width) && result.options.width > 0;
            else if (option == "--height")
                parsed = ParseUnsigned(value, result.options.height) && result.options.height > 0;
            else if (option == "--seed")
                parsed = ParseUnsigned(value, result.options.seed);
            else if (option == "--scene-timeout-ms")
                parsed = ParseUnsigned(value, result.options.sceneBuildTimeoutMs) && result.options.sceneBuildTimeoutMs > 0;
            else if (option == "--output")
            {
                result.options.output = value;
                parsed = !result.options.output.empty();
            }
            else
            {
                result.success = false;
                result.error = "Unknown option: " + std::string(option);
                return result;
            }

            if (!parsed)
            {
                result.success = false;
                result.error = "Invalid value '" + std::string(value) + "' for option " + std::string(option);
                return result;
            }
        }
        return result;
    }
} // namespace ChikaEngine::Benchmark
