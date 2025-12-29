#include "log_system.h"

#include "log_sink.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace ChikaEngine::Debug
{
    LogSystem& LogSystem::Instance()
    {
        static LogSystem instance; // C++11 开始的标准, 静态局部变量初始化为线程安全 很棒
        return instance;
    }

    static std::string GetTimestamp()
    {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto in_time_t = system_clock::to_time_t(now);

        std::tm tm;
#if defined(_MSC_VER)
        localtime_s(&tm, &in_time_t);
#else
        localtime_r(&in_time_t, &tm);
#endif
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        return buffer;
    }

    static uint64_t GetThreadID()
    {
        return std::hash<std::thread::id>{}(std::this_thread::get_id());
    }

    static const char* ToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO ";
        case LogLevel::Warn:
            return "WARN ";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Fatal:
            return "FATAL";
        default:
            return "UNKWN";
        }
    }

    void LogSystem::AddSink(std::unique_ptr<ILogSink> sink)
    {
        std::lock_guard lock(_mutex);
        _sinks.push_back(std::move(sink));
    }

    void LogSystem::Log(LogLevel level, const std::string_view module, const std::string_view message)
    {
        std::ostringstream oss;
        oss << "[" << GetTimestamp() << "]"
            << "[" << ToString(level) << "]"
            << "[" << GetThreadID() << "]"
            << "[" << module << "]" << message;

        std::string finalStr = oss.str();

        // 广播
        std::lock_guard lock(_mutex);
        for (auto& sink : _sinks)
        {
            sink->Write(level, finalStr);
        }
    }

    void Log(LogLevel level, std::string_view module, std::string_view message)
    {
        LogSystem::Instance().Log(level, module, message);
    }
} // namespace ChikaEngine::Debug