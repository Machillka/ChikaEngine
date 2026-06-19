#include "ChikaEngine/benchmark/BenchmarkSceneFactory.hpp"

#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/component/CameraComponent.hpp"
#include "ChikaEngine/component/LightComponent.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/component/Transform.h"
#include "ChikaEngine/gameobject/GameObject.h"
#include "ChikaEngine/scene/scene.hpp"

#include <array>

namespace ChikaEngine::Benchmark
{
    namespace
    {
        /** @brief Resolves a fixture to a stable GUID reference before any scene objects are created. */
        bool ResolveFixture(Asset::AssetManager& assets, const char* path, Asset::AssetType expectedType, Asset::AssetReference& reference, std::string& error)
        {
            const Asset::AssetRecord* record = assets.GetDatabase().FindByPath(path);
            if (!record)
            {
                error = std::string("Missing benchmark fixture: ") + path;
                return false;
            }
            if (record->type != expectedType)
            {
                error = std::string("Benchmark fixture has an unexpected type: ") + path;
                return false;
            }
            reference = Asset::AssetReference(record->guid, expectedType, {}, record->sourcePath.generic_string());
            return true;
        }
    } // namespace

    BenchmarkSceneBuildResult BenchmarkSceneFactory::Build(Framework::Scene& scene, Asset::AssetManager& assets, BenchmarkSceneId sceneId, uint32_t seed, std::chrono::milliseconds timeout)
    {
        m_dynamicObjects.clear();
        const BenchmarkSceneLayout layout = GenerateBenchmarkSceneLayout(sceneId, seed);
        if (timeout <= std::chrono::milliseconds::zero())
            return { .success = false, .error = "Benchmark scene build timeout must be positive" };
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        std::array<Asset::AssetReference, 2> meshes;
        std::array<Asset::AssetReference, 2> materials;
        std::string error;
        if (!layout.objects.empty())
        {
            if (!ResolveFixture(assets, "Assets/Meshes/Box.gltf", Asset::AssetType::Mesh, meshes[0], error) || !ResolveFixture(assets, "Assets/Materials/test.json", Asset::AssetType::Material, materials[0], error))
                return { .success = false, .error = std::move(error) };

            // State-diversity uses two stable resource identities; other scenes reuse the primary pair.
            meshes[1] = meshes[0];
            materials[1] = materials[0];
            if (sceneId == BenchmarkSceneId::StateDiversity)
            {
                if (!ResolveFixture(assets, "Assets/Meshes/Fox.gltf", Asset::AssetType::Mesh, meshes[1], error) || !ResolveFixture(assets, "Assets/Materials/floor.json", Asset::AssetType::Material, materials[1], error))
                    return { .success = false, .error = std::move(error) };
            }
        }

        const Core::GameObjectID cameraId = scene.CreateGameobject("BenchmarkCamera");
        Framework::GameObject* cameraObject = scene.GetGameObject(cameraId);
        cameraObject->transform->position = { layout.worldRadius * 0.65f, layout.worldRadius * 0.85f, layout.worldRadius * 1.15f };
        cameraObject->transform->LookAt(Math::Vector3::zero);
        auto* camera = cameraObject->AddComponent<Framework::CameraComponent>();
        camera->farClip = layout.worldRadius * 5.0f;
        camera->nearClip = 0.1f;
        camera->primary = true;

        const Core::GameObjectID lightId = scene.CreateGameobject("BenchmarkLight");
        Framework::GameObject* lightObject = scene.GetGameObject(lightId);
        lightObject->transform->position = { layout.worldRadius, layout.worldRadius * 1.5f, layout.worldRadius };
        lightObject->transform->LookAt(Math::Vector3::zero);
        auto* light = lightObject->AddComponent<Framework::LightComponent>();
        light->intensity = 2.0f;
        light->castsShadow = false;

        m_dynamicObjects.reserve(sceneId == BenchmarkSceneId::Dynamic5K ? layout.objects.size() : 0);
        for (size_t objectIndex = 0; objectIndex < layout.objects.size(); ++objectIndex)
        {
            if ((objectIndex & 0xffu) == 0u && std::chrono::steady_clock::now() >= deadline)
                return { .success = false, .error = "Benchmark scene build exceeded its timeout" };
            const BenchmarkObjectSpec& spec = layout.objects[objectIndex];
            const Core::GameObjectID objectId = scene.CreateGameobject("BenchmarkObject");
            Framework::GameObject* object = scene.GetGameObject(objectId);
            object->transform->position = spec.position;
            object->transform->scale = spec.scale;
            auto* renderer = object->AddComponent<Framework::MeshRenderer>();
            renderer->SetMeshReference(meshes[spec.meshVariant % meshes.size()]);
            renderer->SetMaterialReference(materials[spec.materialVariant % materials.size()]);
            if (spec.dynamic)
                m_dynamicObjects.push_back({ .id = objectId, .spec = spec });
        }

        return {
            .success = true,
            .sceneConfigHash = layout.stableHash,
            .objectCount = static_cast<uint32_t>(layout.objects.size()),
        };
    }

    void BenchmarkSceneFactory::UpdateDynamicObjects(Framework::Scene& scene, uint64_t simulationFrame) const
    {
        for (const DynamicObject& dynamicObject : m_dynamicObjects)
        {
            if (Framework::GameObject* object = scene.GetGameObject(dynamicObject.id); object && object->transform)
                object->transform->position = ComputeDynamicPosition(dynamicObject.spec, simulationFrame);
        }
    }
} // namespace ChikaEngine::Benchmark
