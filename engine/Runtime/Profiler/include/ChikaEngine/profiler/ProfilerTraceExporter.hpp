#pragma once

#include "ChikaEngine/profiler/ProfilerFrame.hpp"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

namespace ChikaEngine::Profiler
{
    struct ProfilerTraceMetadata
    {
        std::string applicationName = "ChikaEngine";
        std::string buildType;
        std::string platform;
        std::string gitRevision;
        std::unordered_map<std::string, std::string> additional;
    };

    /** @brief Serializes immutable captures into Chrome Trace Event JSON accepted by Perfetto. */
    class ProfilerTraceExporter
    {
      public:
        /** @brief Writes through a temporary file so interrupted exports do not leave partial JSON. */
        static bool Export(const std::filesystem::path& outputPath, std::span<const std::shared_ptr<const ProfilerFrameCapture>> captures, const ProfilerTraceMetadata& metadata, std::string* error = nullptr);
    };
} // namespace ChikaEngine::Profiler
