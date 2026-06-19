#pragma once

#include "ChikaEngine/profiler/ProfilerSession.hpp"

#include <cstdint>

namespace ChikaEngine::Profiler
{
    /** @brief Balances zone events across every normal and exceptional scope exit. */
    class ProfilerScope
    {
      public:
        explicit ProfilerScope(uint32_t nameId) : m_nameId(nameId), m_active(ProfilerSession::Get().BeginZone(nameId)) {}

        ~ProfilerScope()
        {
            if (m_active)
                ProfilerSession::Get().EndZone(m_nameId);
        }

        ProfilerScope(const ProfilerScope&) = delete;
        ProfilerScope& operator=(const ProfilerScope&) = delete;

      private:
        uint32_t m_nameId = 0;
        bool m_active = false;
    };

    /** @brief Guarantees that an opened frame is aggregated when control leaves its loop scope. */
    class ProfilerFrameScope
    {
      public:
        explicit ProfilerFrameScope(uint64_t frameId) : m_frameId(frameId)
        {
            ProfilerSession::Get().BeginFrame(frameId);
        }

        ~ProfilerFrameScope()
        {
            ProfilerSession::Get().EndFrame(m_frameId);
        }

        ProfilerFrameScope(const ProfilerFrameScope&) = delete;
        ProfilerFrameScope& operator=(const ProfilerFrameScope&) = delete;

      private:
        uint64_t m_frameId = 0;
    };
} // namespace ChikaEngine::Profiler
