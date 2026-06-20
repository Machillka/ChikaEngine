#pragma once

#include "ChikaEngine/RenderWorld.hpp"
#include "ChikaEngine/ResourceManager.hpp"

#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ChikaEngine::Render
{
    struct RenderMaterialMetadata
    {
        Resource::MaterialHandle handle;
        PipelineHandle forwardPipeline;
        PipelineHandle gbufferPipeline;
        PipelineHandle shadowPipeline;
    };

    /** @brief Deep-copied frame metadata that workers may read without touching ResourceManager. */
    class RenderResourceView
    {
      public:
        RenderResourceView() = default;
        RenderResourceView(std::vector<Resource::MeshHandle> meshes, std::vector<RenderMaterialMetadata> materials);

        /** @brief Copies only packet-generation metadata on the owner thread before jobs start. */
        static RenderResourceView Build(const RenderWorldSnapshot& snapshot, const Resource::ResourceManager& resources);

        /** @brief Tests whether owner-thread validation accepted this mesh for the frame. */
        bool ContainsMesh(Resource::MeshHandle handle) const;
        /** @brief Returns copied material metadata without consulting ResourceManager. */
        const RenderMaterialMetadata* FindMaterial(Resource::MaterialHandle handle) const;

      private:
        std::unordered_set<Resource::MeshHandle> m_meshes;
        std::unordered_map<Resource::MaterialHandle, RenderMaterialMetadata> m_materials;
    };
} // namespace ChikaEngine::Render
