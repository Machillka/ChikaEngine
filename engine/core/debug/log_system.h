#pragma once
#include "log_sink.h"

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
        void Log(LogLevel level, std::string_view module, std::string_view message);

      private:
        LogSystem() = default;
        std::mutex _mutex;
        std::vector<std::unique_ptr<ILogSink>> _sinks;
    };
    // TODO: 修改 const string& msg -> string_view
    void Log(LogLevel level, std::string_view module, std::string_view message);

} // namespace ChikaEngine::Debug