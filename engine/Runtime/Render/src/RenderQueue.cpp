#include "ChikaEngine/RenderQueue.hpp"

#include "ChikaEngine/RenderSortKey.hpp"
#include "ChikaEngine/math/vector4.h"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"
#include <algorithm>
#include <iterator>
#include <tuple>

namespace ChikaEngine::Render
{
    namespace
    {
        /** @brief 计算对象 Bounds Center 在当前 View 空间中的正向深度。 */
        float ComputeViewDepth(const RenderView& view, const RenderObjectProxy& proxy)
        {
            const Math::Vector4 viewPosition = view.view * Math::Vector4(proxy.bounds.center.x, proxy.bounds.center.y, proxy.bounds.center.z, 1.0f);
            return -viewPosition.z;
        }

        /**
         * @brief Opaque 按 Pipeline、Material、Mesh 聚类，最后用稳定对象 Handle 消除随机顺序。
         */
        bool OpaqueLess(const RenderPacket& lhs, const RenderPacket& rhs)
        {
            return BuildOpaqueSortKey(lhs) < BuildOpaqueSortKey(rhs);
        }

        /**
         * @brief Transparent 保留从远到近顺序，再使用资源和对象身份保证结果稳定。
         */
        bool TransparentLess(const RenderPacket& lhs, const RenderPacket& rhs)
        {
            if (lhs.viewDepth != rhs.viewDepth)
                return lhs.viewDepth > rhs.viewDepth;
            return OpaqueLess(lhs, rhs);
        }

        /** @brief 将排序后共享完整绘制状态的连续 Packet 合并为 Batch。 */
        void BuildBatchesInternal(RenderQueue& queue)
        {
            for (size_t packetIndex = 0; packetIndex < queue.packets.size();)
            {
                const RenderPacket& first = queue.packets[packetIndex];
                size_t packetCount = 1;
                if (first.instancingEligible)
                {
                    while (packetIndex + packetCount < queue.packets.size())
                    {
                        const RenderPacket& candidate = queue.packets[packetIndex + packetCount];
                        if (!candidate.instancingEligible || candidate.pipeline != first.pipeline || candidate.material != first.material || candidate.mesh != first.mesh || candidate.pass != first.pass)
                            break;
                        ++packetCount;
                    }
                }
                queue.batches.push_back({
                    .pass = first.pass,
                    .pipeline = first.pipeline,
                    .material = first.material,
                    .mesh = first.mesh,
                    .firstPacket = packetIndex,
                    .packetCount = packetCount,
                    .instanced = first.instancingEligible,
                });
                packetIndex += packetCount;
            }
        }

        /** @brief 将一个可见 Render Proxy 转换为指定 Pass 的 Render Packet。 */
        void AppendPacket(RenderQueue& queue, const RenderObjectSnapshot& object, RenderPassClass pass, PipelineHandle pipeline, float viewDepth)
        {
            const bool skinned = HasFlag(object.proxy.flags, RenderObjectFlags::Skinned);
            queue.packets.push_back({
                .object = &object,
                .pass = pass,
                .pipeline = pipeline,
                .material = object.proxy.material,
                .mesh = object.proxy.mesh,
                .viewDepth = viewDepth,
                .instancingEligible = !skinned && pass != RenderPassClass::ForwardTransparent,
            });
        }
    } // namespace

    RenderQueueSet BuildRenderQueues(const VisibilityResult& mainVisibility, const VisibilityResult& shadowVisibility, const RenderView& view, const Resource::ResourceManager& resources)
    {
        CHIKA_PROFILE_SCOPE("Renderer.BuildRenderQueues");
        RenderQueueSet queues;
        for (const RenderObjectSnapshot* object : mainVisibility.visibleObjects)
        {
            // 不完整 Proxy 不应进入资源查询与 Draw Queue；Bridge 可在后续帧资源就绪后重新提交。
            if (!object->proxy.mesh.IsValid() || !object->proxy.material.IsValid())
                continue;
            const Resource::MaterialGPU& material = resources.GetMaterial(object->proxy.material);
            const float viewDepth = ComputeViewDepth(view, object->proxy);
            if (HasFlag(object->proxy.flags, RenderObjectFlags::Transparent))
                AppendPacket(queues.forwardTransparent, *object, RenderPassClass::ForwardTransparent, material.forwardPipeline, viewDepth);
            else
            {
                AppendPacket(queues.forwardOpaque, *object, RenderPassClass::ForwardOpaque, material.forwardPipeline, viewDepth);
                AppendPacket(queues.gbufferOpaque, *object, RenderPassClass::GBufferOpaque, material.gbufferPipeline, viewDepth);
            }
        }

        for (const RenderObjectSnapshot* object : shadowVisibility.visibleObjects)
        {
            if (!object->proxy.mesh.IsValid() || !object->proxy.material.IsValid())
                continue;
            const Resource::MaterialGPU& material = resources.GetMaterial(object->proxy.material);
            AppendPacket(queues.shadow, *object, RenderPassClass::Shadow, material.shadowPipeline, 0.0f);
        }

        SortAndBuildRenderBatches(queues.shadow, false);
        SortAndBuildRenderBatches(queues.forwardOpaque, false);
        SortAndBuildRenderBatches(queues.gbufferOpaque, false);
        SortAndBuildRenderBatches(queues.forwardTransparent, true);
        return queues;
    }

    void AppendRenderPackets(RenderQueueSet& queues, std::span<const RenderObjectSnapshot* const> objects, const RenderView& view, const RenderResourceView& resources, bool shadowPass)
    {
        for (const RenderObjectSnapshot* object : objects)
        {
            if (!object || !resources.ContainsMesh(object->proxy.mesh))
                continue;
            const RenderMaterialMetadata* material = resources.FindMaterial(object->proxy.material);
            if (!material)
                continue;
            if (shadowPass)
            {
                AppendPacket(queues.shadow, *object, RenderPassClass::Shadow, material->shadowPipeline, 0.0f);
                continue;
            }

            const float viewDepth = ComputeViewDepth(view, object->proxy);
            if (HasFlag(object->proxy.flags, RenderObjectFlags::Transparent))
                AppendPacket(queues.forwardTransparent, *object, RenderPassClass::ForwardTransparent, material->forwardPipeline, viewDepth);
            else
            {
                AppendPacket(queues.forwardOpaque, *object, RenderPassClass::ForwardOpaque, material->forwardPipeline, viewDepth);
                AppendPacket(queues.gbufferOpaque, *object, RenderPassClass::GBufferOpaque, material->gbufferPipeline, viewDepth);
            }
        }
    }

    RenderQueueSet BuildRenderPacketsSerial(const VisibilityResult& mainVisibility, const VisibilityResult& shadowVisibility, const RenderView& view, const RenderResourceView& resources)
    {
        CHIKA_PROFILE_SCOPE("Renderer.BuildPackets.Serial");
        RenderQueueSet queues;
        AppendRenderPackets(queues, mainVisibility.visibleObjects, view, resources, false);
        AppendRenderPackets(queues, shadowVisibility.visibleObjects, view, resources, true);
        return queues;
    }

    void AppendRenderQueueSet(RenderQueueSet& destination, RenderQueueSet&& source)
    {
        const auto append = [](RenderQueue& target, RenderQueue& local) { target.packets.insert(target.packets.end(), std::make_move_iterator(local.packets.begin()), std::make_move_iterator(local.packets.end())); };
        append(destination.shadow, source.shadow);
        append(destination.forwardOpaque, source.forwardOpaque);
        append(destination.gbufferOpaque, source.gbufferOpaque);
        append(destination.forwardTransparent, source.forwardTransparent);
    }

    void SortAndBuildRenderBatches(RenderQueue& queue, bool transparent)
    {
        std::stable_sort(queue.packets.begin(), queue.packets.end(), transparent ? TransparentLess : OpaqueLess);
        queue.batches.clear();
        BuildBatchesInternal(queue);
    }

    void BuildRenderBatches(RenderQueue& queue)
    {
        queue.batches.clear();
        BuildBatchesInternal(queue);
    }

    void SortAndBuildRenderQueueSetSerial(RenderQueueSet& queues)
    {
        CHIKA_PROFILE_SCOPE("Renderer.Sort.Serial");
        SortAndBuildRenderBatches(queues.shadow, false);
        SortAndBuildRenderBatches(queues.forwardOpaque, false);
        SortAndBuildRenderBatches(queues.gbufferOpaque, false);
        SortAndBuildRenderBatches(queues.forwardTransparent, true);
    }
} // namespace ChikaEngine::Render
