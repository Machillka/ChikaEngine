#pragma once
#include "log_sink.h"

#include <fmt/format.h>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace ChikaEngine::Debug
{
    /*
    Log System
        - Manager Sinks
        - Format messages
    */

    class LogSystem
    {
      public:
        static LogSystem& Instance();
        void AddSink(std::unique_ptr<ILogSink> sink);
        std::string Prefix(LogLevel level, std::string_view module);
        void Log(LogLevel level, std::string_view module, std::string_view message);
        template <typename... Args> void Log(LogLevel level, std::string_view module, std::string_view message, Args&&... args);
        template <typename... Args> void Log(LogLevel level, std::string_view module, fmt::format_string<Args...> message, Args&&... args);

      private:
        LogSystem() = default;
        std::mutex _mutex;
        std::vector<std::unique_ptr<ILogSink>> _sinks;
    };
    // TODO: 修改 const string& msg -> string_view
    void Log(LogLevel level, std::string_view module, std::string_view message);
    template <typename... Args> void Log(LogLevel level, std::string_view module, fmt::format_string<Args...> message, Args&&... args)
    {
        // Format here to avoid relying on member-template linkage across TUs
        std::string formatted = fmt::format(fmt::runtime(message), std::forward<Args>(args)...);
        Log(level, module, formatted);
    }

    template <typename... Args> void Log(LogLevel level, std::string_view module, std::string_view message, Args&&... args)
    {
        std::string formatted = fmt::format(fmt::runtime(message), std::forward<Args>(args)...);
        Log(level, module, formatted);
    }

} // namespace ChikaEngine::Debug