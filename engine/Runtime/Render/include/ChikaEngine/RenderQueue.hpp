#pragma once

#include "ChikaEngine/RenderResourceView.hpp"
#include "ChikaEngine/RenderVisibility.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ChikaEngine::Render
{
    enum class RenderPassClass : uint8_t
    {
        Shadow,
        ForwardOpaque,
        GBufferOpaque,
        ForwardTransparent,
    };

    /**
     * @brief 保存一个可排序的 Pass 绘制单元，替代 Pipeline 对 Render Proxy 的直接遍历。
     */
    struct RenderPacket
    {
        const RenderObjectSnapshot* object = nullptr;
        RenderPassClass pass = RenderPassClass::ForwardOpaque;
        PipelineHandle pipeline;
        Resource::MaterialHandle material;
        Resource::MeshHandle mesh;
        float viewDepth = 0.0f;
        bool instancingEligible = false;
    };

    /**
     * @brief 保存排序后连续共享 Pipeline、Material、Mesh 的 Packet 范围。
     */
    struct RenderBatch
    {
        RenderPassClass pass = RenderPassClass::ForwardOpaque;
        PipelineHandle pipeline;
        Resource::MaterialHandle material;
        Resource::MeshHandle mesh;
        size_t firstPacket = 0;
        size_t packetCount = 0;
        uint32_t firstInstance = 0;
        bool instanced = false;
    };

    /** @brief 描述可合并 Draw 的完整共享状态身份。 */
    struct RenderBatchKey
    {
        RenderPassClass pass = RenderPassClass::ForwardOpaque;
        PipelineHandle pipeline;
        Resource::MaterialHandle material;
        Resource::MeshHandle mesh;

        bool operator==(const RenderBatchKey&) const = default;
    };

    /** @brief RenderQueue 到 DrawIndexed 的可测试提交参数。 */
    struct RenderBatchDrawCommand
    {
        size_t batchIndex = 0;
        RenderPassClass pass = RenderPassClass::ForwardOpaque;
        PipelineHandle pipeline;
        Resource::MaterialHandle material;
        Resource::MeshHandle mesh;
        uint32_t instanceCount = 0;
        uint32_t firstInstance = 0;
        bool instanced = false;
    };

    struct RenderQueue
    {
        std::vector<RenderPacket> packets;
        std::vector<RenderBatch> batches;
    };

    struct RenderQueueSet
    {
        RenderQueue shadow;
        RenderQueue forwardOpaque;
        RenderQueue gbufferOpaque;
        RenderQueue forwardTransparent;
    };

    /**
     * @brief 把主视图和阴影视图可见结果分类、稳定排序并构建 Batch。
     */
    RenderQueueSet BuildRenderQueues(const VisibilityResult& mainVisibility, const VisibilityResult& shadowVisibility, const RenderView& view, const Resource::ResourceManager& resources);

    /** @brief Builds unsorted packets from immutable worker-safe resource metadata. */
    RenderQueueSet BuildRenderPacketsSerial(const VisibilityResult& mainVisibility, const VisibilityResult& shadowVisibility, const RenderView& view, const RenderResourceView& resources);

    /** @brief Appends one main-view or shadow-view range to a caller-owned local queue set. */
    void AppendRenderPackets(RenderQueueSet& queues, std::span<const RenderObjectSnapshot* const> objects, const RenderView& view, const RenderResourceView& resources, bool shadowPass);

    /** @brief Builds the complete shared-state key used by both batch construction and tests. */
    RenderBatchKey BuildRenderBatchKey(const RenderPacket& packet);

    /** @brief Returns true when two sorted packets can share one instanced draw. */
    bool CanMergeRenderPackets(const RenderPacket& first, const RenderPacket& candidate);

    /** @brief Returns the instance count that should be submitted for one batch. */
    uint32_t GetRenderBatchDrawInstanceCount(const RenderBatch& batch);

    /** @brief Returns the firstInstance value that should be submitted for one batch. */
    uint32_t GetRenderBatchDrawFirstInstance(const RenderBatch& batch);

    /** @brief Assigns contiguous instance ranges for every instanced batch and returns the next free slot. */
    uint32_t AssignRenderBatchInstanceRanges(RenderQueue& queue, uint32_t firstInstance);

    /** @brief Converts final batches into mockable draw commands without touching RHI state. */
    std::vector<RenderBatchDrawCommand> BuildRenderBatchDrawCommands(const RenderQueue& queue, bool skipInstancedBatches = false);

    /** @brief Concatenates local queue vectors in deterministic chunk order. */
    void AppendRenderQueueSet(RenderQueueSet& destination, RenderQueueSet&& source);

    /** @brief 对已有 Packet 执行稳定排序和 Batch 构建，供测试与定制 Pass 复用。 */
    void SortAndBuildRenderBatches(RenderQueue& queue, bool transparent);

    /** @brief Builds batches from an already sorted queue without performing another sort. */
    void BuildRenderBatches(RenderQueue& queue);

    /** @brief Sorts every pass with the serial oracle and builds final batches. */
    void SortAndBuildRenderQueueSetSerial(RenderQueueSet& queues);
} // namespace ChikaEngine::Render
