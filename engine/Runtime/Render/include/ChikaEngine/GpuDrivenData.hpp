#pragma once

#include "ChikaEngine/RenderResourceView.hpp"
#include "ChikaEngine/RenderQueue.hpp"
#include "ChikaEngine/RenderSceneView.hpp"
#include "ChikaEngine/RenderVisibility.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace ChikaEngine::Render
{
    inline constexpr uint32_t kGpuDrivenLayoutVersion = 1;
    inline constexpr uint32_t kGpuDrivenCullWorkGroupSize = 64;
    inline constexpr uint32_t kGpuDrivenInvalidVisibleIndex = UINT32_MAX;

    /**
     * @brief CPU/GPU shared static opaque instance record, laid out for std430 storage buffers.
     *
     * Handles are converted to stable 32-bit identities before entering this structure so workers and
     * shaders never observe SlotMap internals or CPU pointers.
     */
    struct alignas(16) GpuInstanceData
    {
        float model[16]{};
        float boundsCenterRadius[4]{};
        float boundsExtentsFlags[4]{};
        uint32_t drawGroupIndex = 0;
        uint32_t objectId = 0;
        uint32_t meshId = 0;
        uint32_t materialId = 0;
    };

    /**
     * @brief CPU/GPU shared draw-group metadata sorted by pipeline, material and mesh identity.
     */
    struct alignas(16) GpuDrawGroup
    {
        uint32_t indexCount = 0;
        uint32_t instanceCount = 0;
        uint32_t firstIndex = 0;
        int32_t vertexOffset = 0;
        uint32_t firstInstance = 0;
        uint32_t visibleOffset = 0;
        uint32_t visibleCount = 0;
        uint32_t pipelineId = 0;
        uint32_t meshId = 0;
        uint32_t materialId = 0;
        uint32_t reserved0 = 0;
        uint32_t reserved1 = 0;
    };

    /**
     * @brief Backend-neutral mirror of VkDrawIndexedIndirectCommand.
     *
     * The structure deliberately keeps Vulkan's 20-byte ABI because the RHI command consumes that exact
     * argument stride for indexed indirect draws.
     */
    struct GpuIndexedIndirectCommand
    {
        uint32_t indexCount = 0;
        uint32_t instanceCount = 0;
        uint32_t firstIndex = 0;
        int32_t vertexOffset = 0;
        uint32_t firstInstance = 0;
    };

    /**
     * @brief Frame constants consumed by the GPU culling shader.
     */
    struct alignas(16) GpuCullFrameData
    {
        float frustumPlanes[24]{};
        uint32_t objectCount = 0;
        uint32_t instanceCapacity = 0;
        uint32_t visibleCapacity = 0;
        uint32_t drawGroupCount = 0;
    };

    static_assert(std::is_trivially_copyable_v<GpuInstanceData>);
    static_assert(std::is_trivially_copyable_v<GpuDrawGroup>);
    static_assert(std::is_trivially_copyable_v<GpuIndexedIndirectCommand>);
    static_assert(std::is_trivially_copyable_v<GpuCullFrameData>);
    static_assert(alignof(GpuInstanceData) == 16);
    static_assert(alignof(GpuDrawGroup) == 16);
    static_assert(sizeof(GpuInstanceData) % 16 == 0);
    static_assert(sizeof(GpuDrawGroup) % 16 == 0);
    static_assert(sizeof(GpuIndexedIndirectCommand) == 20);
    static_assert(sizeof(GpuCullFrameData) == 112);

    struct GpuDrivenBuildConfig
    {
        RenderPassClass pass = RenderPassClass::ForwardOpaque;
        uint32_t maxInstances = UINT32_MAX;
        uint32_t maxDrawGroups = UINT32_MAX;
    };

    struct GpuDrivenFrameData
    {
        std::vector<GpuInstanceData> instances;
        std::vector<uint32_t> visibilityFlags;
        std::vector<uint32_t> visibleInstanceIndices;
        std::vector<GpuDrawGroup> drawGroups;
        std::vector<GpuIndexedIndirectCommand> indirectCommands;
        uint32_t inputStaticOpaqueCount = 0;
        uint32_t cpuVisibleCount = 0;
        uint32_t cpuCulledCount = 0;
        uint32_t skippedInvalidResourceCount = 0;
        bool capacityExceeded = false;
        uint64_t layoutHash = 0;
        uint64_t visibilityHash = 0;
        uint64_t indirectHash = 0;
    };

    /** @brief Builds the compute shader frame constants from the same frustum used by CPU culling. */
    GpuCullFrameData BuildGpuCullFrameData(const RenderView& view, const GpuDrivenFrameData& frameData);
    /** @brief Returns indirect arguments with per-group counters cleared before compute appends visibility. */
    std::vector<GpuIndexedIndirectCommand> BuildGpuDrivenResetIndirectCommands(const GpuDrivenFrameData& frameData);

    /** @brief Returns a stable hash over shared GPU-driven layout constants and ABI sizes. */
    uint64_t ComputeGpuDrivenLayoutHash();
    /** @brief Builds deterministic static opaque GPU-driven buffers and a CPU visibility oracle. */
    GpuDrivenFrameData BuildGpuDrivenFrameData(const RenderSceneView& scene, const RenderView& view, const RenderResourceView& resources, const GpuDrivenBuildConfig& config = {});
    /** @brief Hashes visible instance identities without relying on vector addresses or padding. */
    uint64_t HashGpuDrivenVisibility(std::span<const GpuInstanceData> instances, std::span<const uint32_t> visibleInstanceIndices);
    /** @brief Hashes generated indirect arguments and draw-group metadata for CPU/GPU oracle comparison. */
    uint64_t HashGpuDrivenIndirect(std::span<const GpuDrawGroup> groups, std::span<const GpuIndexedIndirectCommand> commands);
} // namespace ChikaEngine::Render
