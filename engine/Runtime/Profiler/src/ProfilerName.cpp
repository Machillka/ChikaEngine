#include "ChikaEngine/profiler/ProfilerName.hpp"

namespace ChikaEngine::Profiler
{
    ProfilerNameRegistry::ProfilerNameRegistry()
    {
        m_names.emplace_back("<invalid>");
    }

    ProfilerNameRegistry& ProfilerNameRegistry::Instance()
    {
        static auto* registry = new ProfilerNameRegistry();
        return *registry;
    }

    uint32_t ProfilerNameRegistry::Intern(std::string_view name)
    {
        if (name.empty())
            return 0;
        std::lock_guard lock(m_mutex);
        const auto existing = m_ids.find(name);
        if (existing != m_ids.end())
            return existing->second;
        const uint32_t id = static_cast<uint32_t>(m_names.size());
        std::string owned(name);
        m_names.push_back(owned);
        m_ids.emplace(std::move(owned), id);
        return id;
    }

    std::string ProfilerNameRegistry::Resolve(uint32_t id) const
    {
        std::lock_guard lock(m_mutex);
        return id < m_names.size() ? m_names[id] : std::string("<unknown>");
    }

    std::vector<std::string> ProfilerNameRegistry::Snapshot() const
    {
        std::lock_guard lock(m_mutex);
        return m_names;
    }
} // namespace ChikaEngine::Profiler
