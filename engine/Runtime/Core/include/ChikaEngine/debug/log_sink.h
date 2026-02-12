#pragma once

#include <string>
namespace ChikaEngine::Debug
{
    enum class LogLevel
    {
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    // Define the interface to adapt log system
    class ILogSink
    {
      public:
        virtual ~ILogSink() = default;
        virtual void Write(LogLevel level, const std::string& msg) = 0;
    };
} // namespace ChikaEngine::Debug