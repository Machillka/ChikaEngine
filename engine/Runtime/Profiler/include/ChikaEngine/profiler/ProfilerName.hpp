#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ChikaEngine::Profiler
{
    /**
     * @brief Interns profiler names once and exposes stable process-local IDs.
     *
     * Instrumentation macros cache returned IDs in function-local statics, so the mutex is paid
     * on first use rather than on every event.
     */
    class ProfilerNameRegistry
    {
      public:
        static ProfilerNameRegistry& Instance();
        uint32_t Intern(std::string_view name);
        std::string Resolve(uint32_t id) const;
        std::vector<std::string> Snapshot() const;

      private:
        ProfilerNameRegistry();

        mutable std::mutex m_mutex;
        std::unordered_map<std::string, uint32_t> m_ids;
        std::vector<std::string> m_names;
    };
} // namespace ChikaEngine::Profiler
