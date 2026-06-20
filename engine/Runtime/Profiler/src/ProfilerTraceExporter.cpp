#include "ChikaEngine/profiler/ProfilerTraceExporter.hpp"

#include "ChikaEngine/profiler/ProfilerEvent.hpp"
#include "ChikaEngine/profiler/ProfilerName.hpp"

#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <system_error>
#if defined(_WIN32)
#include <Windows.h>
#endif

namespace ChikaEngine::Profiler
{
    namespace
    {
        constexpr uint32_t kCpuProcessId = 1;
        constexpr uint32_t kGpuProcessId = 2;

        /** @brief Converts the profiler nanosecond clock to Trace Event microseconds. */
        double ToMicroseconds(uint64_t nanoseconds)
        {
            return static_cast<double>(nanoseconds) / 1'000.0;
        }

        /** @brief Resolves an interned name while preserving diagnostics for invalid IDs. */
        std::string ResolveName(uint32_t nameId)
        {
            std::string name = ProfilerNameRegistry::Instance().Resolve(nameId);
            return name.empty() ? "<unnamed>" : name;
        }

        /** @brief Adds process and thread metadata events understood by Perfetto. */
        void AppendTrackMetadata(nlohmann::json& events, uint32_t processId, uint32_t threadId, std::string_view processName, std::string_view threadName)
        {
            events.push_back({ { "name", "process_name" }, { "ph", "M" }, { "pid", processId }, { "tid", 0 }, { "args", { { "name", processName } } } });
            events.push_back({ { "name", "thread_name" }, { "ph", "M" }, { "pid", processId }, { "tid", threadId }, { "args", { { "name", threadName } } } });
        }

        /** @brief Atomically replaces the destination where the host platform provides that operation. */
        void PublishTemporary(const std::filesystem::path& temporaryPath, const std::filesystem::path& outputPath)
        {
#if defined(_WIN32)
            if (!MoveFileExW(temporaryPath.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "failed to publish profiler trace");
#else
            std::filesystem::rename(temporaryPath, outputPath);
#endif
        }
    } // namespace

    bool ProfilerTraceExporter::Export(const std::filesystem::path& outputPath, std::span<const std::shared_ptr<const ProfilerFrameCapture>> captures, const ProfilerTraceMetadata& metadata, std::string* error)
    {
        const std::filesystem::path temporaryPath = outputPath.string() + ".tmp";
        try
        {
            nlohmann::json root;
            root["displayTimeUnit"] = "ms";
            root["metadata"] = { { "application", metadata.applicationName }, { "buildType", metadata.buildType }, { "platform", metadata.platform }, { "gitRevision", metadata.gitRevision }, { "eventSchemaVersion", kProfilerEventSchemaVersion }, { "additional", metadata.additional } };
            nlohmann::json& events = root["traceEvents"] = nlohmann::json::array();

            std::unordered_map<uint32_t, std::string> emittedCpuThreads;
            std::unordered_map<uint32_t, bool> emittedGpuQueues;
            for (const auto& capture : captures)
            {
                if (!capture)
                    continue;

                for (const ProfilerThreadInfo& thread : capture->threads)
                {
                    if (!emittedCpuThreads.contains(thread.threadId))
                    {
                        AppendTrackMetadata(events, kCpuProcessId, thread.threadId, "ChikaEngine CPU", thread.name);
                        emittedCpuThreads[thread.threadId] = thread.name;
                    }
                }

                events.push_back({ { "name", "Frame" }, { "cat", "frame" }, { "ph", "X" }, { "pid", kCpuProcessId }, { "tid", 0 }, { "ts", ToMicroseconds(capture->startNs) }, { "dur", ToMicroseconds(capture->durationNs) }, { "args", { { "frameId", capture->frameId }, { "droppedEvents", capture->droppedEventCount }, { "malformedZones", capture->malformedZoneCount }, { "hitch", capture->hitch } } } });

                for (const ProfilerZone& zone : capture->zones)
                {
                    events.push_back({ { "name", ResolveName(zone.nameId) }, { "cat", "cpu" }, { "ph", "X" }, { "pid", kCpuProcessId }, { "tid", zone.threadId }, { "ts", ToMicroseconds(zone.startNs) }, { "dur", ToMicroseconds(zone.inclusiveNs) }, { "args", { { "exclusiveNs", zone.exclusiveNs }, { "depth", zone.depth }, { "malformed", zone.malformed } } } });
                }
                for (const ProfilerCounter& counter : capture->counters)
                {
                    events.push_back({ { "name", ResolveName(counter.nameId) }, { "cat", "counter" }, { "ph", "C" }, { "pid", kCpuProcessId }, { "tid", counter.threadId }, { "ts", ToMicroseconds(counter.timestampNs) }, { "args", { { "value", counter.value } } } });
                }
                for (const ProfilerInstant& instant : capture->instants)
                {
                    events.push_back({ { "name", ResolveName(instant.nameId) }, { "cat", "instant" }, { "ph", "i" }, { "s", "t" }, { "pid", kCpuProcessId }, { "tid", instant.threadId }, { "ts", ToMicroseconds(instant.timestampNs) }, { "args", { { "payload", instant.payload } } } });
                }

                std::map<uint32_t, uint64_t> gpuCursor;
                for (const ProfilerGpuZone& zone : capture->gpuZones)
                {
                    if (!emittedGpuQueues.contains(zone.queueId))
                    {
                        AppendTrackMetadata(events, kGpuProcessId, zone.queueId, "ChikaEngine GPU", "GPU Queue " + std::to_string(zone.queueId));
                        emittedGpuQueues[zone.queueId] = true;
                    }
                    uint64_t& cursor = gpuCursor[zone.queueId];
                    if (cursor == 0)
                        cursor = capture->startNs;
                    events.push_back({ { "name", ResolveName(zone.nameId) }, { "cat", "gpu" }, { "ph", "X" }, { "pid", kGpuProcessId }, { "tid", zone.queueId }, { "ts", ToMicroseconds(cursor) }, { "dur", ToMicroseconds(zone.durationNs) }, { "args", { { "frameId", capture->frameId }, { "valid", zone.valid }, { "clockDomain", "correlated-duration" } } } });
                    cursor += zone.durationNs;
                }
            }

            if (const auto parent = outputPath.parent_path(); !parent.empty())
                std::filesystem::create_directories(parent);
            {
                std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
                if (!stream)
                    throw std::runtime_error("failed to open profiler trace output");
                stream << root.dump(2);
                if (!stream)
                    throw std::runtime_error("failed to write profiler trace output");
            }

            PublishTemporary(temporaryPath, outputPath);
            return true;
        }
        catch (const std::exception& exception)
        {
            if (error)
                *error = exception.what();
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            return false;
        }
    }
} // namespace ChikaEngine::Profiler
