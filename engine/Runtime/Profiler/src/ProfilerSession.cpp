#include "ChikaEngine/profiler/ProfilerSession.hpp"

#include "ChikaEngine/profiler/ProfilerClock.hpp"
#include "ChikaEngine/profiler/ProfilerName.hpp"

#include <utility>

namespace ChikaEngine::Profiler
{
    ProfilerSession::ProfilerSession() : m_threads(65'536), m_history(300) {}

    ProfilerSession& ProfilerSession::Get()
    {
        static ProfilerSession* session = new ProfilerSession();
        return *session;
    }

    void ProfilerSession::Initialize(const ProfilerConfig& config)
    {
        std::scoped_lock lock(m_frameMutex, m_gpuMutex);
        m_enabled.store(false, std::memory_order_release);
        m_currentFrameId.store(kInvalidProfilerFrameId, std::memory_order_release);
        m_threads.SetDefaultBufferCapacity(config.threadBufferCapacity);
        m_threads.DrainAll();
        m_aggregator.Clear();
        m_history.Clear();
        m_history.SetCapacity(config.historyCapacity);
        m_pendingGpu.clear();
        m_frameStartNs = 0;
        m_hitchThresholdNs = config.hitchThresholdNs;
        m_initialized.store(true, std::memory_order_release);
        m_enabled.store(config.enabled, std::memory_order_release);
    }

    void ProfilerSession::Shutdown()
    {
        m_enabled.store(false, std::memory_order_release);
        m_currentFrameId.store(kInvalidProfilerFrameId, std::memory_order_release);
        std::scoped_lock lock(m_frameMutex, m_gpuMutex);
        m_threads.DrainAll();
        m_aggregator.Clear();
        m_pendingGpu.clear();
        m_initialized.store(false, std::memory_order_release);
    }

    void ProfilerSession::SetEnabled(bool enabled)
    {
        m_enabled.store(enabled && IsInitialized(), std::memory_order_release);
    }

    bool ProfilerSession::IsEnabled() const
    {
        return m_enabled.load(std::memory_order_acquire);
    }

    bool ProfilerSession::IsInitialized() const
    {
        return m_initialized.load(std::memory_order_acquire);
    }

    void ProfilerSession::SetCurrentThreadName(std::string_view name)
    {
        m_threads.SetCurrentThreadName(name);
    }

    void ProfilerSession::BeginFrame(uint64_t frameId)
    {
        if (!IsEnabled())
            return;

        std::lock_guard lock(m_frameMutex);
        m_frameStartNs = ProfilerClock::NowNanoseconds();
        m_currentFrameId.store(frameId, std::memory_order_release);
        ProfilerEvent event;
        event.timestampNs = m_frameStartNs;
        event.frameId = frameId;
        event.type = ProfilerEventType::FrameMark;
        event.flags = ProfilerEventFlags::FrameBegin;
        Record(event);
    }

    void ProfilerSession::EndFrame(uint64_t frameId)
    {
        if (m_currentFrameId.load(std::memory_order_acquire) != frameId)
            return;

        std::lock_guard lock(m_frameMutex);
        const uint64_t endNs = ProfilerClock::NowNanoseconds();
        ProfilerEvent event;
        event.timestampNs = endNs;
        event.frameId = frameId;
        event.type = ProfilerEventType::FrameMark;
        Record(event);
        m_currentFrameId.store(kInvalidProfilerFrameId, std::memory_order_release);

        auto immutable = m_aggregator.Aggregate(frameId, m_frameStartNs, endNs, m_threads.DrainAll(), m_hitchThresholdNs);
        auto capture = std::make_shared<ProfilerFrameCapture>(*immutable);
        {
            std::lock_guard gpuLock(m_gpuMutex);
            AttachPendingGpu(*capture);
            m_history.Add(std::move(capture));
        }
    }

    bool ProfilerSession::BeginZone(uint32_t nameId)
    {
        if (!IsEnabled())
            return false;
        ProfilerEvent event;
        event.timestampNs = ProfilerClock::NowNanoseconds();
        event.nameId = nameId;
        event.type = ProfilerEventType::ZoneBegin;
        return Record(event);
    }

    void ProfilerSession::EndZone(uint32_t nameId)
    {
        ProfilerEvent event;
        event.timestampNs = ProfilerClock::NowNanoseconds();
        event.nameId = nameId;
        event.type = ProfilerEventType::ZoneEnd;
        Record(event);
    }

    void ProfilerSession::RecordCounter(uint32_t nameId, int64_t value)
    {
        if (!IsEnabled())
            return;
        ProfilerEvent event;
        event.timestampNs = ProfilerClock::NowNanoseconds();
        event.nameId = nameId;
        event.type = ProfilerEventType::Counter;
        event.payload = PackProfilerInteger(value);
        Record(event);
    }

    void ProfilerSession::RecordCounter(uint32_t nameId, double value)
    {
        if (!IsEnabled())
            return;
        ProfilerEvent event;
        event.timestampNs = ProfilerClock::NowNanoseconds();
        event.nameId = nameId;
        event.type = ProfilerEventType::Counter;
        event.flags = ProfilerEventFlags::FloatingPointPayload;
        event.payload = PackProfilerDouble(value);
        Record(event);
    }

    void ProfilerSession::RecordInstant(uint32_t nameId)
    {
        if (!IsEnabled())
            return;
        ProfilerEvent event;
        event.timestampNs = ProfilerClock::NowNanoseconds();
        event.nameId = nameId;
        event.type = ProfilerEventType::Instant;
        Record(event);
    }

    void ProfilerSession::SubmitGpuTimings(std::span<const ProfilerGpuTiming> timings)
    {
        std::lock_guard lock(m_gpuMutex);
        for (const ProfilerGpuTiming& timing : timings)
        {
            ProfilerGpuZone zone;
            zone.nameId = ProfilerNameRegistry::Instance().Intern(timing.name);
            zone.queueId = timing.queueId;
            zone.durationNs = timing.durationNs;
            zone.valid = timing.valid;

            if (const auto existing = m_history.Find(timing.frameId))
            {
                auto replacement = std::make_shared<ProfilerFrameCapture>(*existing);
                replacement->gpuZones.push_back(zone);
                m_history.Replace(std::move(replacement));
                continue;
            }
            m_pendingGpu[timing.frameId].push_back(zone);
        }
    }

    ProfilerHistory& ProfilerSession::GetHistory()
    {
        return m_history;
    }

    const ProfilerHistory& ProfilerSession::GetHistory() const
    {
        return m_history;
    }

    bool ProfilerSession::Record(ProfilerEvent event)
    {
        if (!IsEnabled())
            return false;
        const uint64_t frameId = m_currentFrameId.load(std::memory_order_acquire);
        if (frameId == kInvalidProfilerFrameId)
            return false;
        event.frameId = frameId;
        event.threadId = m_threads.GetCurrentThreadId();
        return m_threads.GetOrRegisterCurrentThread()->TryPush(event);
    }

    void ProfilerSession::AttachPendingGpu(ProfilerFrameCapture& capture)
    {
        const auto pending = m_pendingGpu.find(capture.frameId);
        if (pending == m_pendingGpu.end())
            return;
        capture.gpuZones = std::move(pending->second);
        m_pendingGpu.erase(pending);
    }
} // namespace ChikaEngine::Profiler
