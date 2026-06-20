#include "ChikaEngine/jobs/JobProfiler.hpp"

#include "ChikaEngine/profiler/ProfilerName.hpp"
#include "ChikaEngine/profiler/ProfilerSession.hpp"

namespace ChikaEngine::Jobs
{
    namespace
    {
        void Instant(uint32_t nameId, JobHandle handle)
        {
            Profiler::ProfilerSession::Get().RecordInstant(nameId, handle.RawValue());
        }

        void Counter(uint32_t nameId, uint64_t value)
        {
            Profiler::ProfilerSession::Get().RecordCounter(nameId, static_cast<int64_t>(value));
        }
    } // namespace

    void JobProfiler::Enqueued(JobHandle handle)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.Enqueue");
        Instant(nameId, handle);
    }

    void JobProfiler::Started(JobHandle handle)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.Start");
        Instant(nameId, handle);
    }

    void JobProfiler::Completed(JobHandle handle)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.Complete");
        Instant(nameId, handle);
    }

    void JobProfiler::Stolen(JobHandle handle)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.Steal");
        Instant(nameId, handle);
    }

    void JobProfiler::Cancelled(JobHandle handle)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.Cancel");
        Instant(nameId, handle);
    }

    void JobProfiler::Failed(JobHandle handle)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.Failure");
        Instant(nameId, handle);
    }

    void JobProfiler::QueueDepth(uint32_t depth)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.QueueDepth");
        Counter(nameId, depth);
    }

    void JobProfiler::ActiveWorkers(uint32_t count)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.ActiveWorkers");
        Counter(nameId, count);
    }

    void JobProfiler::SleepingWorkers(uint32_t count)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.SleepingWorkers");
        Counter(nameId, count);
    }

    void JobProfiler::StealCount(uint64_t count)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.SuccessfulSteals");
        Counter(nameId, count);
    }

    void JobProfiler::QueueWait(uint64_t nanoseconds)
    {
        static const uint32_t nameId = Profiler::ProfilerNameRegistry::Instance().Intern("Jobs.QueueWaitNs");
        Counter(nameId, nanoseconds);
    }
} // namespace ChikaEngine::Jobs
