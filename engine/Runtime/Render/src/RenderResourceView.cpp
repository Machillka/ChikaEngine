#include "ChikaEngine/RenderResourceView.hpp"

#include "ChikaEngine/profiler/ProfilerMacros.hpp"

#include <utility>

namespace ChikaEngine::Render
{
    RenderResourceView::RenderResourceView(std::vector<Resource::MeshHandle> meshes, std::vector<RenderMaterialMetadata> materials)
        : RenderResourceView([&]()
                             {
                                 std::vector<RenderMeshMetadata> metadata;
                                 metadata.reserve(meshes.size());
                                 for (Resource::MeshHandle mesh : meshes)
                                     metadata.push_back({ .handle = mesh });
                                 return metadata;
                             }(),
                             std::move(materials))
    {
    }

    RenderResourceView::RenderResourceView(std::vector<RenderMeshMetadata> meshes, std::vector<RenderMaterialMetadata> materials)
    {
        m_meshes.reserve(meshes.size());
        for (const RenderMeshMetadata& mesh : meshes)
            m_meshes.insert_or_assign(mesh.handle, mesh);
        m_materials.reserve(materials.size());
        for (const RenderMaterialMetadata& material : materials)
            m_materials.insert_or_assign(material.handle, material);
    }

    RenderResourceView RenderResourceView::Build(const RenderWorldSnapshot& snapshot, const Resource::ResourceManager& resources)
    {
        CHIKA_PROFILE_SCOPE("Renderer.BuildResourceView");
        std::vector<RenderMeshMetadata> meshes;
        std::vector<RenderMaterialMetadata> materials;
        std::unordered_set<Resource::MeshHandle> seenMeshes;
        std::unordered_set<Resource::MaterialHandle> seenMaterials;
        meshes.reserve(snapshot.objects.size());
        materials.reserve(snapshot.objects.size());

        for (const RenderObjectSnapshot& object : snapshot.objects)
        {
            const Resource::MeshGPU* mesh = object.proxy.mesh.IsValid() && !seenMeshes.contains(object.proxy.mesh) ? resources.TryGetMesh(object.proxy.mesh) : nullptr;
            if (mesh)
            {
                seenMeshes.insert(object.proxy.mesh);
                meshes.push_back({
                    .handle = object.proxy.mesh,
                    .indexCount = mesh->indexCount,
                    .isUint32 = mesh->isUint32,
                });
            }
            if (!object.proxy.material.IsValid() || seenMaterials.contains(object.proxy.material))
                continue;
            const Resource::MaterialGPU* material = resources.TryGetMaterial(object.proxy.material);
            if (!material)
                continue;
            seenMaterials.insert(object.proxy.material);
            materials.push_back({
                .handle = object.proxy.material,
                .forwardPipeline = material->forwardPipeline,
                .gbufferPipeline = material->gbufferPipeline,
                .shadowPipeline = material->shadowPipeline,
            });
        }
        return RenderResourceView(std::move(meshes), std::move(materials));
    }

    bool RenderResourceView::ContainsMesh(Resource::MeshHandle handle) const
    {
        return m_meshes.contains(handle);
    }

    const RenderMeshMetadata* RenderResourceView::FindMesh(Resource::MeshHandle handle) const
    {
        const auto iterator = m_meshes.find(handle);
        return iterator == m_meshes.end() ? nullptr : &iterator->second;
    }

    const RenderMaterialMetadata* RenderResourceView::FindMaterial(Resource::MaterialHandle handle) const
    {
        const auto iterator = m_materials.find(handle);
        return iterator == m_materials.end() ? nullptr : &iterator->second;
    }
} // namespace ChikaEngine::Render
