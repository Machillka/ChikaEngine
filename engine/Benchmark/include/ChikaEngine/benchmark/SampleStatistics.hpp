#pragma once

#include <cstddef>
#include <span>

namespace ChikaEngine::Benchmark
{
    struct SampleDistribution
    {
        size_t count = 0;
        double minimum = 0.0;
        double maximum = 0.0;
        double mean = 0.0;
        double p50 = 0.0;
        double p95 = 0.0;
        double p99 = 0.0;
    };

    /**
     * @brief Computes deterministic nearest-rank percentiles from finite non-negative samples.
     *
     * Invalid timings are rejected instead of silently biasing performance reports.
     */
    SampleDistribution ComputeSampleDistribution(std::span<const double> samples);
} // namespace ChikaEngine::Benchmark
