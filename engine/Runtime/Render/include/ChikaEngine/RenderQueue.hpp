#pragma once

#include "ChikaEngine/RenderVisibility.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include <cstddef>
#include <cstdint>
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

    /** @brief 对已有 Packet 执行稳定排序和 Batch 构建，供测试与定制 Pass 复用。 */
    void SortAndBuildRenderBatches(RenderQueue& queue, bool transparent);
} // namespace ChikaEngine::Render
