#pragma once

#include "ChikaEngine/AssetReference.hpp"
#include "ChikaEngine/benchmark/BenchmarkSceneLayout.hpp"
#include "ChikaEngine/base/UIDGenerator.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace ChikaEngine::Asset
{
    class AssetManager;
}

namespace ChikaEngine::Framework
{
    class Scene;
}

namespace ChikaEngine::Benchmark
{
    struct BenchmarkSceneBuildResult
    {
        bool success = false;
        std::string error;
        uint64_t sceneConfigHash = 0;
        uint32_t objectCount = 0;
    };

    /**
     * @brief Materializes deterministic layout data into Framework objects and tracks dynamic transforms.
     *
     * Asset resolution happens before object creation, keeping missing fixtures from producing partial scenes.
     */
    class BenchmarkSceneFactory
    {
      public:
        BenchmarkSceneBuildResult Build(Framework::Scene& scene, Asset::AssetManager& assets, BenchmarkSceneId sceneId, uint32_t seed, std::chrono::milliseconds timeout = std::chrono::seconds(120));
        /** @brief Updates only the known dynamic subset using an absolute simulation frame. */
        void UpdateDynamicObjects(Framework::Scene& scene, uint64_t simulationFrame) const;

      private:
        struct DynamicObject
        {
            Core::GameObjectID id = Core::InvalidGameObjectID;
            BenchmarkObjectSpec spec;
        };

        std::vector<DynamicObject> m_dynamicObjects;
    };
} // namespace ChikaEngine::Benchmark
