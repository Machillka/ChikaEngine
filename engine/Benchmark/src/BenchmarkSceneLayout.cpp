#include "ChikaEngine/benchmark/BenchmarkSceneLayout.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace ChikaEngine::Benchmark
{
    namespace
    {
        /** @brief Combines scalar layout fields using stable FNV-1a bytes for run identity. */
        void HashU32(uint64_t& hash, uint32_t value)
        {
            for (uint32_t shift = 0; shift < 32; shift += 8)
            {
                hash ^= (value >> shift) & 0xffu;
                hash *= 1099511628211ull;
            }
        }

        /** @brief Adds a float by its IEEE bit pattern so serialized layout values define the hash. */
        void HashFloat(uint64_t& hash, float value)
        {
            HashU32(hash, std::bit_cast<uint32_t>(value));
        }

        /** @brief Returns the fixed object count represented by a public scene id. */
        uint32_t ObjectCount(BenchmarkSceneId scene)
        {
            switch (scene)
            {
            case BenchmarkSceneId::Empty:
                return 0;
            case BenchmarkSceneId::Instance1K:
                return 1'000;
            case BenchmarkSceneId::Instance10K:
                return 10'000;
            case BenchmarkSceneId::Instance50K:
                return 50'000;
            case BenchmarkSceneId::Instance100K:
                return 100'000;
            case BenchmarkSceneId::StateDiversity:
                return 10'000;
            case BenchmarkSceneId::Dynamic5K:
                return 5'000;
            }
            return 0;
        }
    } // namespace

    BenchmarkSceneLayout GenerateBenchmarkSceneLayout(BenchmarkSceneId scene, uint32_t seed)
    {
        BenchmarkSceneLayout layout{ .scene = scene, .seed = seed };
        const uint32_t count = ObjectCount(scene);
        layout.objects.reserve(count);

        const uint32_t side = count == 0 ? 1 : static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(count))));
        constexpr float spacing = 2.25f;
        const float halfExtent = static_cast<float>(side - 1) * spacing * 0.5f;
        layout.worldRadius = std::max(10.0f, halfExtent * 1.5f);

        // A tiny integer generator provides reproducible phase variation without global random state.
        uint32_t randomState = seed == 0 ? 0x9e3779b9u : seed;
        for (uint32_t index = 0; index < count; ++index)
        {
            randomState = randomState * 1664525u + 1013904223u;
            const uint32_t row = index / side;
            const uint32_t column = index % side;
            BenchmarkObjectSpec object;
            object.position = { static_cast<float>(column) * spacing - halfExtent, 0.0f, static_cast<float>(row) * spacing - halfExtent };
            object.dynamic = scene == BenchmarkSceneId::Dynamic5K;
            object.dynamicPhase = static_cast<float>(randomState & 0xffffu) * (6.28318530718f / 65535.0f);
            if (scene == BenchmarkSceneId::StateDiversity)
            {
                object.meshVariant = index % 2;
                object.materialVariant = (index / 2) % 2;
                object.scale.y = 0.30f + static_cast<float>(index % 5) * 0.08f;
            }
            layout.objects.push_back(object);
        }

        uint64_t hash = 14695981039346656037ull;
        HashU32(hash, static_cast<uint32_t>(scene));
        HashU32(hash, seed);
        HashU32(hash, count);
        for (const auto& object : layout.objects)
        {
            HashFloat(hash, object.position.x);
            HashFloat(hash, object.position.y);
            HashFloat(hash, object.position.z);
            HashFloat(hash, object.scale.x);
            HashFloat(hash, object.scale.y);
            HashFloat(hash, object.scale.z);
            HashU32(hash, object.meshVariant);
            HashU32(hash, object.materialVariant);
            HashFloat(hash, object.dynamicPhase);
            HashU32(hash, object.dynamic ? 1u : 0u);
        }
        layout.stableHash = hash;
        return layout;
    }

    Math::Vector3 ComputeDynamicPosition(const BenchmarkObjectSpec& object, uint64_t simulationFrame)
    {
        const float time = static_cast<float>(simulationFrame) * (1.0f / 60.0f);
        Math::Vector3 position = object.position;
        position.y += 0.75f * std::sin(time * 1.7f + object.dynamicPhase);
        return position;
    }
} // namespace ChikaEngine::Benchmark
