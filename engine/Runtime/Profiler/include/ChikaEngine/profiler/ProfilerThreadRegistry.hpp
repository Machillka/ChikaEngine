#pragma once

#include "ChikaEngine/profiler/ProfilerEventBuffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ChikaEngine::Profiler
{
    struct ProfilerThreadBatch
    {
        uint32_t threadId = 0;
        std::string threadName;
        std::vector<ProfilerEvent> events;
        uint64_t droppedEventCount = 0;
        bool threadExited = false;
    };

    /**
     * @brief Registers producer threads and owns their SPSC buffers for the process lifetime.
     *
     * Registration may lock and allocate once per thread. Event writes use a thread-local raw
     * buffer pointer and never acquire the registry mutex.
     */
    class ProfilerThreadRegistry
    {
      public:
        // Public only so the translation unit can hold a weak TLS token; implementation remains opaque.
        struct State;

        explicit ProfilerThreadRegistry(size_t defaultBufferCapacity = 65'536);
        ~ProfilerThreadRegistry();

        ProfilerThreadRegistry(const ProfilerThreadRegistry&) = delete;
        ProfilerThreadRegistry& operator=(const ProfilerThreadRegistry&) = delete;

        ProfilerEventBuffer* GetOrRegisterCurrentThread();
        uint32_t GetCurrentThreadId();
        void SetCurrentThreadName(std::string_view name);
        std::vector<ProfilerThreadBatch> DrainAll();
        void SetDefaultBufferCapacity(size_t capacity);
        size_t GetRegisteredThreadCount() const;

      private:
        std::shared_ptr<State> m_state;
    };
} // namespace ChikaEngine::Profiler
