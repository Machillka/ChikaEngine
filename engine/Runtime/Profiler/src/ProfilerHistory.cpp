#include "ChikaEngine/profiler/ProfilerHistory.hpp"

#include <algorithm>

namespace ChikaEngine::Profiler
{
    ProfilerHistory::ProfilerHistory(size_t capacity) : m_capacity(std::max<size_t>(1, capacity)) {}

    void ProfilerHistory::SetCapacity(size_t capacity)
    {
        std::lock_guard lock(m_mutex);
        m_capacity = std::max<size_t>(1, capacity);
        TrimLocked();
    }

    void ProfilerHistory::Add(std::shared_ptr<const ProfilerFrameCapture> capture)
    {
        if (!capture)
            return;
        std::lock_guard lock(m_mutex);
        m_frames.push_back(std::move(capture));
        TrimLocked();
    }

    bool ProfilerHistory::Replace(std::shared_ptr<const ProfilerFrameCapture> capture)
    {
        if (!capture)
            return false;
        std::lock_guard lock(m_mutex);
        bool replaced = false;
        for (auto& frame : m_frames)
        {
            if (frame->frameId == capture->frameId)
            {
                frame = capture;
                replaced = true;
            }
        }
        if (m_pinned.contains(capture->frameId))
        {
            m_pinned[capture->frameId] = capture;
            replaced = true;
        }
        return replaced;
    }

    std::shared_ptr<const ProfilerFrameCapture> ProfilerHistory::Latest() const
    {
        std::lock_guard lock(m_mutex);
        return m_frames.empty() ? nullptr : m_frames.back();
    }

    std::shared_ptr<const ProfilerFrameCapture> ProfilerHistory::Find(uint64_t frameId) const
    {
        std::lock_guard lock(m_mutex);
        const auto pinned = m_pinned.find(frameId);
        if (pinned != m_pinned.end())
            return pinned->second;
        const auto frame = std::ranges::find(m_frames, frameId, [](const auto& capture) { return capture->frameId; });
        return frame == m_frames.end() ? nullptr : *frame;
    }

    std::vector<std::shared_ptr<const ProfilerFrameCapture>> ProfilerHistory::Snapshot() const
    {
        std::lock_guard lock(m_mutex);
        return { m_frames.begin(), m_frames.end() };
    }

    bool ProfilerHistory::Pin(uint64_t frameId)
    {
        std::lock_guard lock(m_mutex);
        const auto frame = std::ranges::find(m_frames, frameId, [](const auto& capture) { return capture->frameId; });
        if (frame == m_frames.end())
            return false;
        m_pinned[frameId] = *frame;
        return true;
    }

    void ProfilerHistory::Unpin(uint64_t frameId)
    {
        std::lock_guard lock(m_mutex);
        m_pinned.erase(frameId);
    }

    void ProfilerHistory::Clear()
    {
        std::lock_guard lock(m_mutex);
        m_frames.clear();
        m_pinned.clear();
    }

    void ProfilerHistory::TrimLocked()
    {
        while (m_frames.size() > m_capacity)
            m_frames.pop_front();
    }
} // namespace ChikaEngine::Profiler
