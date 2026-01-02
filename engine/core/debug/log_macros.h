#pragma once
#include "debug/log_system.h"
//::指从全局开始查找命名空间
// Debug 日志：只在 Debug 模式输出
#ifdef CHIKA_DEBUG
#define LOG_DEBUG(module, msg) ::ChikaEngine::Debug::Log(::ChikaEngine::Debug::LogLevel::Debug, module, msg)
#else
// Release 模式下，Debug 日志直接变成空操作，不产生任何开销
#define LOG_DEBUG(module, msg)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)sizeof(module);                                                                                          \
        (void)sizeof(msg);                                                                                             \
    } while (0)
#endif

// Info / Warn / Error / Fatal definition
#define LOG_INFO(module, msg) ::ChikaEngine::Debug::Log(::ChikaEngine::Debug::LogLevel::Info, module, msg)

#define LOG_WARN(module, msg) ::ChikaEngine::Debug::Log(::ChikaEngine::Debug::LogLevel::Warn, module, msg)

#define LOG_ERROR(module, msg) ::ChikaEngine::Debug::Log(::ChikaEngine::Debug::LogLevel::Error, module, msg)

#define LOG_FATAL(module, msg) ::ChikaEngine::Debug::Log(::ChikaEngine::Debug::LogLevel::Fatal, module, msg)
