#include "ChikaEngine/jobs/JobQueue.hpp"

#include <algorithm>

namespace ChikaEngine::Jobs
{
    JobQueue::JobQueue(size_t capacity) : m_capacity(std::max<size_t>(1, capacity)) {}

    bool JobQueue::Push(JobHandle handle)
    {
        std::lock_guard lock(m_mutex);
        if (m_queue.size() >= m_capacity)
            return false;
        m_queue.push_back(handle);
        return true;
    }

    bool JobQueue::PopLocal(JobHandle& handle)
    {
        std::lock_guard lock(m_mutex);
        if (m_queue.empty())
            return false;
        handle = m_queue.back();
        m_queue.pop_back();
        return true;
    }

    bool JobQueue::Steal(JobHandle& handle)
    {
        std::lock_guard lock(m_mutex);
        if (m_queue.empty())
            return false;
        handle = m_queue.front();
        m_queue.pop_front();
        return true;
    }

    size_t JobQueue::Size() const
    {
        std::lock_guard lock(m_mutex);
        return m_queue.size();
    }

    void JobQueue::Clear()
    {
        std::lock_guard lock(m_mutex);
        m_queue.clear();
    }
} // namespace ChikaEngine::Jobs
