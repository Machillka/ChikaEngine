#pragma once

#ifndef CHIKA_ENABLE_PROFILING
#define CHIKA_ENABLE_PROFILING 0
#endif

#define CHIKA_PROFILE_CONCAT_IMPL(lhs, rhs) lhs##rhs
#define CHIKA_PROFILE_CONCAT(lhs, rhs) CHIKA_PROFILE_CONCAT_IMPL(lhs, rhs)

#if CHIKA_ENABLE_PROFILING

#include "ChikaEngine/profiler/ProfilerName.hpp"
#include "ChikaEngine/profiler/ProfilerScope.hpp"

#define CHIKA_PROFILE_SCOPE(name) \
    static const uint32_t CHIKA_PROFILE_CONCAT(chikaProfileName_, __LINE__) = ::ChikaEngine::Profiler::ProfilerNameRegistry::Instance().Intern(name); \
    ::ChikaEngine::Profiler::ProfilerScope CHIKA_PROFILE_CONCAT(chikaProfileScope_, __LINE__)(CHIKA_PROFILE_CONCAT(chikaProfileName_, __LINE__))

#define CHIKA_PROFILE_FUNCTION() CHIKA_PROFILE_SCOPE(__func__)

#define CHIKA_PROFILE_FRAME(frameId) ::ChikaEngine::Profiler::ProfilerFrameScope CHIKA_PROFILE_CONCAT(chikaProfileFrame_, __LINE__)(frameId)

#define CHIKA_PROFILE_COUNTER(name, value) \
    do \
    { \
        static const uint32_t CHIKA_PROFILE_CONCAT(chikaProfileCounterName_, __LINE__) = ::ChikaEngine::Profiler::ProfilerNameRegistry::Instance().Intern(name); \
        ::ChikaEngine::Profiler::ProfilerSession::Get().RecordCounter(CHIKA_PROFILE_CONCAT(chikaProfileCounterName_, __LINE__), value); \
    } while (false)

#define CHIKA_PROFILE_INSTANT(name) \
    do \
    { \
        static const uint32_t CHIKA_PROFILE_CONCAT(chikaProfileInstantName_, __LINE__) = ::ChikaEngine::Profiler::ProfilerNameRegistry::Instance().Intern(name); \
        ::ChikaEngine::Profiler::ProfilerSession::Get().RecordInstant(CHIKA_PROFILE_CONCAT(chikaProfileInstantName_, __LINE__)); \
    } while (false)

#else

#define CHIKA_PROFILE_SCOPE(name) ((void)0)
#define CHIKA_PROFILE_FUNCTION() ((void)0)
#define CHIKA_PROFILE_FRAME(frameId) ((void)0)
#define CHIKA_PROFILE_COUNTER(name, value) ((void)0)
#define CHIKA_PROFILE_INSTANT(name) ((void)0)

#endif
