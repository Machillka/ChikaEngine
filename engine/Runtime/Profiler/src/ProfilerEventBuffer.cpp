#include "ChikaEngine/profiler/ProfilerEventBuffer.hpp"

#include <algorithm>

namespace ChikaEngine::Profiler
{
    ProfilerEventBuffer::ProfilerEventBuffer(size_t capacity) : m_events(std::max<size_t>(2, capacity)) {}

    bool ProfilerEventBuffer::TryPush(const ProfilerEvent& event)
    {
        const uint64_t write = m_writeIndex.load(std::memory_order_relaxed);
        const uint64_t read = m_readIndex.load(std::memory_order_acquire);
        if (write - read >= m_events.size())
        {
            m_droppedCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        m_events[write % m_events.size()] = event;
        m_writeIndex.store(write + 1, std::memory_order_release);
        return true;
    }

    size_t ProfilerEventBuffer::Drain(std::vector<ProfilerEvent>& destination)
    {
        const uint64_t read = m_readIndex.load(std::memory_order_relaxed);
        const uint64_t write = m_writeIndex.load(std::memory_order_acquire);
        destination.reserve(destination.size() + static_cast<size_t>(write - read));
        for (uint64_t index = read; index < write; ++index)
            destination.push_back(m_events[index % m_events.size()]);
        m_readIndex.store(write, std::memory_order_release);
        return static_cast<size_t>(write - read);
    }

    uint64_t ProfilerEventBuffer::ConsumeDroppedCount()
    {
        return m_droppedCount.exchange(0, std::memory_order_acq_rel);
    }
} // namespace ChikaEngine::Profiler
