#pragma once

#include "ChikaEngine/benchmark/BenchmarkOptions.hpp"
#include "ChikaEngine/math/vector3.h"

#include <cstdint>
#include <vector>

namespace ChikaEngine::Benchmark
{
    struct BenchmarkObjectSpec
    {
        Math::Vector3 position;
        Math::Vector3 scale{ 0.45f, 0.45f, 0.45f };
        uint32_t meshVariant = 0;
        uint32_t materialVariant = 0;
        float dynamicPhase = 0.0f;
        bool dynamic = false;
    };

    struct BenchmarkSceneLayout
    {
        BenchmarkSceneId scene = BenchmarkSceneId::Empty;
        uint32_t seed = 0;
        std::vector<BenchmarkObjectSpec> objects;
        uint64_t stableHash = 0;
        float worldRadius = 10.0f;
    };

    /** @brief Generates a deterministic CPU-only workload description for the selected scene. */
    BenchmarkSceneLayout GenerateBenchmarkSceneLayout(BenchmarkSceneId scene, uint32_t seed);
    /** @brief Computes the dynamic position without relying on frame delta accumulation. */
    Math::Vector3 ComputeDynamicPosition(const BenchmarkObjectSpec& object, uint64_t simulationFrame);
} // namespace ChikaEngine::Benchmark
