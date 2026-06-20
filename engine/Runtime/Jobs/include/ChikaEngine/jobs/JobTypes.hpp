#pragma once

#include <cstdint>
#include <stdexcept>

namespace ChikaEngine::Jobs
{
    struct JobHandle
    {
        static constexpr uint32_t InvalidIndex = UINT32_MAX;

        uint32_t index = InvalidIndex;
        uint32_t generation = 0;

        constexpr bool IsValid() const
        {
            return index != InvalidIndex && generation != 0;
        }

        constexpr uint64_t RawValue() const
        {
            return static_cast<uint64_t>(generation) << 32u | index;
        }

        static constexpr JobHandle Invalid()
        {
            return {};
        }

        bool operator==(const JobHandle&) const = default;
    };

    enum class JobState : uint8_t
    {
        Free,
        Created,
        Queued,
        Running,
        Completed,
        Cancelled,
        Failed,
    };

    enum class JobTarget : uint8_t
    {
        AnyWorker,
        MainThread,
    };

    enum class JobFailurePolicy : uint8_t
    {
        Cancel,
        Propagate,
        RunAnyway,
    };

    enum class JobShutdownPolicy : uint8_t
    {
        Drain,
        CancelPending,
    };

    struct JobSystemStatistics
    {
        uint64_t submittedJobs = 0;
        uint64_t completedJobs = 0;
        uint64_t failedJobs = 0;
        uint64_t cancelledJobs = 0;
        uint64_t localPops = 0;
        uint64_t injectionPops = 0;
        uint64_t stealAttempts = 0;
        uint64_t successfulSteals = 0;
        uint64_t sleepCount = 0;
        uint64_t queueWaitNanoseconds = 0;
        uint64_t executionNanoseconds = 0;
        uint32_t activeWorkers = 0;
        uint32_t sleepingWorkers = 0;
        uint32_t workerCount = 0;
        uint32_t queuedJobs = 0;
        double workerUtilization = 0.0;
    };

    class InvalidJobHandleError : public std::runtime_error
    {
      public:
        InvalidJobHandleError() : std::runtime_error("invalid or stale job handle") {}
    };

    class JobCancelledError : public std::runtime_error
    {
      public:
        JobCancelledError() : std::runtime_error("job was cancelled") {}
    };

    class JobCapacityError : public std::runtime_error
    {
      public:
        explicit JobCapacityError(const char* message) : std::runtime_error(message) {}
    };
} // namespace ChikaEngine::Jobs
