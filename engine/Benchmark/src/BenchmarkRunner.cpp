#include "ChikaEngine/benchmark/BenchmarkRunner.hpp"

#include <stdexcept>

namespace ChikaEngine::Benchmark
{
    BenchmarkRunner::BenchmarkRunner(BenchmarkResult initialResult) : m_result(std::move(initialResult))
    {
        if (m_result.configuration.sampleFrames == 0)
            throw std::invalid_argument("BenchmarkRunner requires at least one sample frame");
        m_result.samples.reserve(m_result.configuration.sampleFrames);
        if (m_result.configuration.warmupFrames == 0)
            m_state = BenchmarkRunnerState::Sampling;
    }

    void BenchmarkRunner::ObserveFrame(const BenchmarkFrameObservation& observation)
    {
        if (m_state == BenchmarkRunnerState::Complete)
            return;
        ++m_observedFrames;

        // GPU queries complete on a later frame; update the original CPU sample by submission id.
        if (observation.completedGpuFrame)
        {
            const auto sample = m_sampleIndexByFrame.find(observation.completedGpuFrame->frameIndex);
            if (sample != m_sampleIndexByFrame.end())
            {
                BenchmarkFrameSample& target = m_result.samples[sample->second];
                target.gpuTimeMs = observation.completedGpuFrame->gpuTimeMs;
                target.gpuTimingAvailable = true;
            }
        }

        if (m_state == BenchmarkRunnerState::Warmup)
        {
            if (++m_warmupObserved >= m_result.configuration.warmupFrames)
                m_state = BenchmarkRunnerState::Sampling;
            return;
        }

        if (m_state == BenchmarkRunnerState::Sampling)
        {
            const size_t sampleIndex = m_result.samples.size();
            m_result.samples.push_back({
                .frameIndex = observation.frameIndex,
                .frameTimeMs = observation.frameTimeMs,
                .engineTickCpuTimeMs = observation.engineTickCpuTimeMs,
                .renderGraphCpuTimeMs = observation.renderGraphCpuTimeMs,
                .renderStatistics = observation.renderStatistics,
            });
            m_sampleIndexByFrame.emplace(observation.frameIndex, sampleIndex);
            if (m_result.samples.size() == m_result.configuration.sampleFrames)
            {
                if (m_result.configuration.flushFrames == 0)
                {
                    ComputeBenchmarkAggregates(m_result);
                    m_state = BenchmarkRunnerState::Complete;
                }
                else
                {
                    m_state = BenchmarkRunnerState::Flushing;
                }
            }
            return;
        }

        if (++m_flushObserved >= m_result.configuration.flushFrames)
        {
            ComputeBenchmarkAggregates(m_result);
            m_state = BenchmarkRunnerState::Complete;
        }
    }
} // namespace ChikaEngine::Benchmark
