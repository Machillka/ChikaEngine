#pragma once

#include "ChikaEngine/benchmark/BenchmarkResult.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace ChikaEngine::Benchmark
{
    enum class BenchmarkRunnerState
    {
        Warmup,
        Sampling,
        Flushing,
        Complete,
    };

    struct BenchmarkFrameObservation
    {
        uint64_t frameIndex = 0;
        double frameTimeMs = 0.0;
        double engineTickCpuTimeMs = 0.0;
        double renderGraphCpuTimeMs = 0.0;
        Render::RenderFrameStatistics renderStatistics;
        struct CompletedGpuFrame
        {
            uint64_t frameIndex = 0;
            double gpuTimeMs = 0.0;
        };
        std::optional<CompletedGpuFrame> completedGpuFrame;
    };

    /**
     * @brief Owns the warmup, sampling and delayed-GPU flush lifecycle for one benchmark run.
     *
     * The state machine makes exact sample counts testable and prevents warmup or flush frames
     * from entering the persisted distribution.
     */
    class BenchmarkRunner
    {
      public:
        explicit BenchmarkRunner(BenchmarkResult initialResult);

        /** @brief Advances one completed frame and records it only when the runner is sampling. */
        void ObserveFrame(const BenchmarkFrameObservation& observation);
        BenchmarkRunnerState GetState() const
        {
            return m_state;
        }
        bool IsComplete() const
        {
            return m_state == BenchmarkRunnerState::Complete;
        }
        uint64_t GetObservedFrameCount() const
        {
            return m_observedFrames;
        }
        const BenchmarkResult& GetResult() const
        {
            return m_result;
        }

      private:
        BenchmarkResult m_result;
        BenchmarkRunnerState m_state = BenchmarkRunnerState::Warmup;
        uint64_t m_observedFrames = 0;
        uint32_t m_warmupObserved = 0;
        uint32_t m_flushObserved = 0;
        std::unordered_map<uint64_t, size_t> m_sampleIndexByFrame;
    };
} // namespace ChikaEngine::Benchmark
