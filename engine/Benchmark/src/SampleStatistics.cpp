#include "ChikaEngine/benchmark/SampleStatistics.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace ChikaEngine::Benchmark
{
    SampleDistribution ComputeSampleDistribution(std::span<const double> samples)
    {
        SampleDistribution result;
        if (samples.empty())
            return result;

        std::vector<double> sorted(samples.begin(), samples.end());
        for (double sample : sorted)
        {
            if (!std::isfinite(sample) || sample < 0.0)
                throw std::invalid_argument("Benchmark samples must be finite and non-negative");
        }
        std::ranges::sort(sorted);

        // Nearest-rank percentiles are simple to reproduce in reports and never synthesize a timing.
        const auto percentile = [&sorted](double probability)
        {
            const size_t rank = std::max<size_t>(1, static_cast<size_t>(std::ceil(probability * static_cast<double>(sorted.size()))));
            return sorted[std::min(rank - 1, sorted.size() - 1)];
        };

        result.count = sorted.size();
        result.minimum = sorted.front();
        result.maximum = sorted.back();
        result.mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / static_cast<double>(sorted.size());
        result.p50 = percentile(0.50);
        result.p95 = percentile(0.95);
        result.p99 = percentile(0.99);
        return result;
    }
} // namespace ChikaEngine::Benchmark
