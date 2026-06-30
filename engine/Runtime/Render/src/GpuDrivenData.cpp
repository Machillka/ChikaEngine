#include "ChikaEngine/GpuDrivenData.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <tuple>

namespace ChikaEngine::Render
{
    namespace
    {
        constexpr uint64_t kFnvOffset = 14695981039346656037ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;

        /**
         * @brief Mixes one integer into a stable FNV-1a hash without depending on host object padding.
         */
        void HashValue(uint64_t& hash, uint64_t value)
        {
            for (uint32_t index = 0; index < 8; ++index)
            {
                hash ^= (value >> (index * 8u)) & 0xFFu;
                hash *= kFnvPrime;
            }
        }

        /**
         * @brief Quantizes floats before hashing so tiny math differences do not poison oracle output.
         */
        uint32_t QuantizeFloat(float value)
        {
            return static_cast<uint32_t>(static_cast<int32_t>(value * 1024.0f));
        }

        /**
         * @brief Selects the graphics pipeline identity used to group static opaque draws.
         */
        PipelineHandle SelectPipeline(const RenderMaterialMetadata& material, RenderPassClass pass)
        {
            switch (pass)
            {
            case RenderPassClass::Shadow:
                return material.shadowPipeline;
            case RenderPassClass::GBufferOpaque:
                return material.gbufferPipeline;
            case RenderPassClass::ForwardOpaque:
            case RenderPassClass::ForwardTransparent:
                return material.forwardPipeline;
            }
            return {};
        }

        struct DrawGroupKey
        {
            uint32_t pipeline = 0;
            uint32_t material = 0;
            uint32_t mesh = 0;

            bool operator<(const DrawGroupKey& other) const
            {
                return std::tie(pipeline, material, mesh) < std::tie(other.pipeline, other.material, other.mesh);
            }
        };

        struct PendingInstance
        {
            GpuInstanceData data;
            uint32_t objectId = 0;
            bool visible = false;
        };

        struct PendingGroup
        {
            const RenderMeshMetadata* mesh = nullptr;
            std::vector<PendingInstance> instances;
        };

        /**
         * @brief Copies the engine Mat4 value into the explicit GPU array representation.
         */
        void CopyMatrix(const Math::Mat4& matrix, float (&output)[16])
        {
            std::memcpy(output, matrix.m.data(), sizeof(float) * 16u);
        }

        /**
         * @brief Converts one render instance into the shared GPU instance ABI.
         */
        GpuInstanceData MakeGpuInstance(const RenderInstanceData& instance)
        {
            GpuInstanceData result;
            CopyMatrix(instance.transform.Transposed(), result.model);
            result.boundsCenterRadius[0] = instance.bounds.center.x;
            result.boundsCenterRadius[1] = instance.bounds.center.y;
            result.boundsCenterRadius[2] = instance.bounds.center.z;
            result.boundsCenterRadius[3] = instance.bounds.sphereRadius;
            result.boundsExtentsFlags[0] = instance.bounds.extents.x;
            result.boundsExtentsFlags[1] = instance.bounds.extents.y;
            result.boundsExtentsFlags[2] = instance.bounds.extents.z;
            result.boundsExtentsFlags[3] = static_cast<float>(static_cast<uint32_t>(instance.flags));
            result.objectId = instance.handle.raw_value;
            result.meshId = instance.mesh.raw_value;
            result.materialId = instance.material.raw_value;
            result.drawGroupIndex = 0;
            return result;
        }
    } // namespace

    uint64_t ComputeGpuDrivenLayoutHash()
    {
        uint64_t hash = kFnvOffset;
        HashValue(hash, kGpuDrivenLayoutVersion);
        HashValue(hash, kGpuDrivenCullWorkGroupSize);
        HashValue(hash, sizeof(GpuInstanceData));
        HashValue(hash, alignof(GpuInstanceData));
        HashValue(hash, offsetof(GpuInstanceData, model));
        HashValue(hash, offsetof(GpuInstanceData, boundsCenterRadius));
        HashValue(hash, offsetof(GpuInstanceData, boundsExtentsFlags));
        HashValue(hash, offsetof(GpuInstanceData, drawGroupIndex));
        HashValue(hash, sizeof(GpuDrawGroup));
        HashValue(hash, offsetof(GpuDrawGroup, indexCount));
        HashValue(hash, offsetof(GpuDrawGroup, firstInstance));
        HashValue(hash, offsetof(GpuDrawGroup, visibleOffset));
        HashValue(hash, sizeof(GpuIndexedIndirectCommand));
        HashValue(hash, sizeof(GpuCullFrameData));
        return hash;
    }

    GpuDrivenFrameData BuildGpuDrivenFrameData(const RenderSceneView& scene, const RenderView& view, const RenderResourceView& resources, const GpuDrivenBuildConfig& config)
    {
        GpuDrivenFrameData result;
        result.layoutHash = ComputeGpuDrivenLayoutHash();
        const std::span<const RenderInstanceData> staticOpaque = scene.GetStaticOpaqueInstances();
        result.inputStaticOpaqueCount = static_cast<uint32_t>(staticOpaque.size());

        std::map<DrawGroupKey, PendingGroup> pendingGroups;
        const ViewFrustum frustum = ViewFrustum::FromViewProjection(view.viewProjection);

        for (uint32_t sourceOrder = 0; sourceOrder < staticOpaque.size(); ++sourceOrder)
        {
            const RenderInstanceData& instance = staticOpaque[sourceOrder];
            const RenderMeshMetadata* mesh = resources.FindMesh(instance.mesh);
            const RenderMaterialMetadata* material = resources.FindMaterial(instance.material);
            if (!mesh || !material)
            {
                ++result.skippedInvalidResourceCount;
                continue;
            }

            const PipelineHandle pipeline = SelectPipeline(*material, config.pass);
            if (!pipeline.IsValid())
            {
                ++result.skippedInvalidResourceCount;
                continue;
            }

            DrawGroupKey key{
                .pipeline = pipeline.raw_value,
                .material = material->handle.raw_value,
                .mesh = mesh->handle.raw_value,
            };
            PendingGroup& group = pendingGroups[key];
            group.mesh = mesh;
            group.instances.push_back({
                .data = MakeGpuInstance(instance),
                .objectId = instance.handle.raw_value,
                .visible = IsRenderInstanceVisible(instance, view, frustum, false),
            });
        }

        if (pendingGroups.size() > config.maxDrawGroups)
            result.capacityExceeded = true;

        result.drawGroups.reserve(pendingGroups.size());
        result.instances.reserve(staticOpaque.size());
        result.visibilityFlags.reserve(staticOpaque.size());
        result.indirectCommands.reserve(pendingGroups.size());

        for (auto& [key, pendingGroup] : pendingGroups)
        {
            if (result.drawGroups.size() >= config.maxDrawGroups)
                break;

            std::ranges::stable_sort(pendingGroup.instances, {}, &PendingInstance::objectId);
            const uint32_t groupIndex = static_cast<uint32_t>(result.drawGroups.size());
            const uint32_t firstInstance = static_cast<uint32_t>(result.instances.size());
            const uint32_t visibleOffset = static_cast<uint32_t>(result.visibleInstanceIndices.size());
            uint32_t visibleCount = 0;
            result.visibleInstanceIndices.resize(result.visibleInstanceIndices.size() + pendingGroup.instances.size(), kGpuDrivenInvalidVisibleIndex);

            for (PendingInstance& instance : pendingGroup.instances)
            {
                if (result.instances.size() >= config.maxInstances)
                {
                    result.capacityExceeded = true;
                    break;
                }

                instance.data.drawGroupIndex = groupIndex;
                const uint32_t instanceIndex = static_cast<uint32_t>(result.instances.size());
                result.instances.push_back(instance.data);
                result.visibilityFlags.push_back(instance.visible ? 1u : 0u);
                if (instance.visible)
                {
                    result.visibleInstanceIndices[visibleOffset + visibleCount] = instanceIndex;
                    ++visibleCount;
                    ++result.cpuVisibleCount;
                }
                else
                    ++result.cpuCulledCount;
            }

            const uint32_t instanceCount = static_cast<uint32_t>(result.instances.size()) - firstInstance;
            result.drawGroups.push_back({
                .indexCount = pendingGroup.mesh ? pendingGroup.mesh->indexCount : 0u,
                .instanceCount = instanceCount,
                .firstIndex = pendingGroup.mesh ? pendingGroup.mesh->firstIndex : 0u,
                .vertexOffset = pendingGroup.mesh ? pendingGroup.mesh->vertexOffset : 0,
                .firstInstance = firstInstance,
                .visibleOffset = visibleOffset,
                .visibleCount = visibleCount,
                .pipelineId = key.pipeline,
                .meshId = key.mesh,
                .materialId = key.material,
            });
            result.indirectCommands.push_back({
                .indexCount = pendingGroup.mesh ? pendingGroup.mesh->indexCount : 0u,
                .instanceCount = visibleCount,
                .firstIndex = pendingGroup.mesh ? pendingGroup.mesh->firstIndex : 0u,
                .vertexOffset = pendingGroup.mesh ? pendingGroup.mesh->vertexOffset : 0,
                .firstInstance = visibleOffset,
            });
        }

        result.visibilityHash = HashGpuDrivenVisibility(result.instances, result.visibleInstanceIndices);
        result.indirectHash = HashGpuDrivenIndirect(result.drawGroups, result.indirectCommands);
        return result;
    }

    uint64_t HashGpuDrivenVisibility(std::span<const GpuInstanceData> instances, std::span<const uint32_t> visibleInstanceIndices)
    {
        uint64_t hash = kFnvOffset;
        uint64_t validVisibleCount = 0;
        for (uint32_t index : visibleInstanceIndices)
        {
            if (index < instances.size())
                ++validVisibleCount;
        }
        HashValue(hash, validVisibleCount);
        for (uint32_t index : visibleInstanceIndices)
        {
            if (index >= instances.size())
                continue;
            const GpuInstanceData& instance = instances[index];
            HashValue(hash, instance.objectId);
            HashValue(hash, instance.drawGroupIndex);
            HashValue(hash, QuantizeFloat(instance.boundsCenterRadius[0]));
            HashValue(hash, QuantizeFloat(instance.boundsCenterRadius[1]));
            HashValue(hash, QuantizeFloat(instance.boundsCenterRadius[2]));
        }
        return hash;
    }

    GpuCullFrameData BuildGpuCullFrameData(const RenderView& view, const GpuDrivenFrameData& frameData)
    {
        GpuCullFrameData result;
        const ViewFrustum frustum = ViewFrustum::FromViewProjection(view.viewProjection);
        const auto& planes = frustum.GetPlanes();
        for (uint32_t index = 0; index < planes.size(); ++index)
        {
            result.frustumPlanes[index * 4u + 0u] = planes[index].normal.x;
            result.frustumPlanes[index * 4u + 1u] = planes[index].normal.y;
            result.frustumPlanes[index * 4u + 2u] = planes[index].normal.z;
            result.frustumPlanes[index * 4u + 3u] = planes[index].distance;
        }
        result.objectCount = static_cast<uint32_t>(frameData.instances.size());
        result.instanceCapacity = static_cast<uint32_t>(frameData.instances.size());
        result.visibleCapacity = static_cast<uint32_t>(frameData.visibleInstanceIndices.size());
        result.drawGroupCount = static_cast<uint32_t>(frameData.drawGroups.size());
        return result;
    }

    std::vector<GpuIndexedIndirectCommand> BuildGpuDrivenResetIndirectCommands(const GpuDrivenFrameData& frameData)
    {
        std::vector<GpuIndexedIndirectCommand> result = frameData.indirectCommands;
        for (GpuIndexedIndirectCommand& command : result)
            command.instanceCount = 0;
        return result;
    }

    uint64_t HashGpuDrivenIndirect(std::span<const GpuDrawGroup> groups, std::span<const GpuIndexedIndirectCommand> commands)
    {
        uint64_t hash = kFnvOffset;
        HashValue(hash, groups.size());
        HashValue(hash, commands.size());
        for (const GpuDrawGroup& group : groups)
        {
            HashValue(hash, group.pipelineId);
            HashValue(hash, group.materialId);
            HashValue(hash, group.meshId);
            HashValue(hash, group.firstInstance);
            HashValue(hash, group.instanceCount);
            HashValue(hash, group.visibleOffset);
            HashValue(hash, group.visibleCount);
        }
        for (const GpuIndexedIndirectCommand& command : commands)
        {
            HashValue(hash, command.indexCount);
            HashValue(hash, command.instanceCount);
            HashValue(hash, command.firstIndex);
            HashValue(hash, static_cast<uint32_t>(command.vertexOffset));
            HashValue(hash, command.firstInstance);
        }
        return hash;
    }
} // namespace ChikaEngine::Render
