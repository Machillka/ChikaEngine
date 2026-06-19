#pragma once

#include "ChikaEngine/profiler/ProfilerAggregator.hpp"
#include "ChikaEngine/profiler/ProfilerHistory.hpp"

#include <atomic>
#include <cstddef>
#include <concepts>
#include <cstdint>
#include <mutex>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ChikaEngine::Profiler
{
    struct ProfilerConfig
    {
        bool enabled = true;
        size_t historyCapacity = 300;
        size_t threadBufferCapacity = 65'536;
        uint64_t hitchThresholdNs = 33'333'333;
    };

    /** @brief Owns profiling collection, aggregation, history, and delayed GPU correlation. */
    class ProfilerSession
    {
      public:
        /** @brief Returns the process-lifetime profiling service without imposing shutdown order. */
        static ProfilerSession& Get();

        /** @brief Resets capture state and applies capacities before producer threads register. */
        void Initialize(const ProfilerConfig& config = {});

        /** @brief Stops event collection and clears unfinished aggregation state. */
        void Shutdown();

        /** @brief Changes the hot-path runtime gate without changing compile-time instrumentation. */
        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        bool IsInitialized() const;

        /** @brief Associates a readable label with the calling thread's stable profiler ID. */
        void SetCurrentThreadName(std::string_view name);

        /** @brief Opens one engine frame and emits its begin marker. */
        void BeginFrame(uint64_t frameId);

        /** @brief Closes, aggregates, and publishes one immutable frame capture. */
        void EndFrame(uint64_t frameId);

        /** @brief Emits the begin half of a nested CPU zone when collection is active. */
        bool BeginZone(uint32_t nameId);

        /** @brief Emits the matching end half of a previously opened CPU zone. */
        void EndZone(uint32_t nameId);

        /** @brief Emits an integer counter sample into the current frame. */
        void RecordCounter(uint32_t nameId, int64_t value);

        /** @brief Normalizes all integral counter types into the fixed signed event payload. */
        template <std::integral T> void RecordCounter(uint32_t nameId, T value)
        {
            RecordCounter(nameId, static_cast<int64_t>(value));
        }

        /** @brief Emits a floating-point counter sample into the current frame. */
        void RecordCounter(uint32_t nameId, double value);

        /** @brief Emits a zero-duration point event into the current frame. */
        void RecordInstant(uint32_t nameId);

        /** @brief Correlates delayed GPU query results with their originating CPU frame ID. */
        void SubmitGpuTimings(std::span<const ProfilerGpuTiming> timings);

        ProfilerHistory& GetHistory();
        const ProfilerHistory& GetHistory() const;

      private:
        ProfilerSession();
        bool Record(ProfilerEvent event);
        /** @brief Moves delayed GPU zones while the caller holds m_gpuMutex. */
        void AttachPendingGpu(ProfilerFrameCapture& capture);

        ProfilerThreadRegistry m_threads;
        ProfilerAggregator m_aggregator;
        ProfilerHistory m_history;
        std::atomic<bool> m_initialized{ false };
        std::atomic<bool> m_enabled{ false };
        std::atomic<uint64_t> m_currentFrameId{ kInvalidProfilerFrameId };
        uint64_t m_frameStartNs = 0;
        uint64_t m_hitchThresholdNs = 33'333'333;
        std::mutex m_frameMutex;
        std::mutex m_gpuMutex;
        std::unordered_map<uint64_t, std::vector<ProfilerGpuZone>> m_pendingGpu;
    };
} // namespace ChikaEngine::Profiler
