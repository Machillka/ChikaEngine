#include "ChikaEngine/GpuDrivenData.hpp"
#include "ChikaEngine/GpuDrivenRenderGraph.hpp"
#include "ChikaEngine/GpuDrivenSubmission.hpp"
#include "ChikaEngine/GpuDrivenValidation.hpp"
#include "ChikaEngine/RenderPath.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using namespace ChikaEngine;

    int g_failures = 0;

    /** @brief Records all failures so one run reports the full Phase 4 contract surface. */
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    class MinimalCommandList final : public Render::IRHICommandList
    {
      public:
        void Begin() override {}
        void End() override {}
        void SetDebugName(std::string_view) override {}
        void BeginDebugLabel(std::string_view, const float[4]) override {}
        void EndDebugLabel() override {}
        void BeginTimestampScope(std::string_view) override {}
        void EndTimestampScope() override {}
        void BeginRendering(const std::vector<Render::RenderingAttachment>&, const Render::RenderingAttachment*) override {}
        void EndRendering() override {}
        void InsertTextureBarrier(Render::TextureHandle, Render::ResourceState, Render::ResourceState, const Render::TextureSubresourceRange&) override {}
        void InsertBufferBarrier(Render::BufferHandle, Render::ResourceState, Render::ResourceState, const Render::BufferRange&) override {}
        void BindPipeline(Render::PipelineHandle) override {}
        void BindResources(const Render::ResourceBindingGroup&) override {}
        void BindVertexBuffer(Render::BufferHandle, uint64_t) override {}
        void BindIndexBuffer(Render::BufferHandle, uint64_t, bool) override {}
        void PushConstants(std::string_view, const void*, uint32_t) override {}
        void CopyBuffer(Render::BufferHandle, Render::BufferHandle, uint64_t) override {}
        void CopyBufferToTexture(Render::BufferHandle, Render::TextureHandle, uint32_t, uint32_t) override {}
        void Draw(uint32_t, uint32_t, uint32_t, uint32_t) override {}
        void DrawIndexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {}
        void DrawIndirect(Render::BufferHandle, uint64_t, uint32_t, uint32_t) override {}
        void DrawIndexedIndirect(Render::BufferHandle, uint64_t offset, uint32_t drawCount, uint32_t stride) override
        {
            ++indexedIndirectCalls;
            lastOffset = offset;
            lastDrawCount = drawCount;
            lastStride = stride;
        }
        void Dispatch(uint32_t, uint32_t, uint32_t) override {}

        uint32_t indexedIndirectCalls = 0;
        uint64_t lastOffset = 0;
        uint32_t lastDrawCount = 0;
        uint32_t lastStride = 0;
    };

    class MinimalDevice final : public Render::IRHIDevice
    {
      public:
        void Initialize(const Render::RHI_InitParams&) override {}
        void Shutdown() override {}
        void BeginFrame() override {}
        void EndFrame() override {}
        Render::BufferHandle CreateBuffer(const Render::BufferDesc&) override
        {
            return Render::BufferHandle::FromParts(nextHandle++, 1);
        }
        Render::TextureHandle CreateTexture(const Render::TextureDesc&) override
        {
            return Render::TextureHandle::FromParts(nextHandle++, 1);
        }
        Render::SamplerHandle CreateSampler(const Render::SamplerDesc&) override
        {
            return Render::SamplerHandle::FromParts(nextHandle++, 1);
        }
        Render::TextureViewHandle CreateTextureView(const Render::TextureViewDesc&) override
        {
            return Render::TextureViewHandle::FromParts(nextHandle++, 1);
        }
        Render::ShaderHandle CreateShader(const Render::ShaderDesc&) override
        {
            return Render::ShaderHandle::FromParts(nextHandle++, 1);
        }
        Render::PipelineHandle CreateGraphicsPipeline(const Render::PipelineDesc&) override
        {
            return Render::PipelineHandle::FromParts(nextHandle++, 1);
        }
        Render::PipelineHandle CreateComputePipeline(const Render::ComputePipelineDesc&) override
        {
            return Render::PipelineHandle::FromParts(nextHandle++, 1);
        }
        void* GetMappedData(Render::BufferHandle) override
        {
            return nullptr;
        }
        void SetDebugName(Render::BufferHandle, std::string_view) override {}
        void SetDebugName(Render::TextureHandle, std::string_view) override {}
        void SetDebugName(Render::ShaderHandle, std::string_view) override {}
        void SetDebugName(Render::PipelineHandle, std::string_view) override {}
        void SetDebugName(Render::SamplerHandle, std::string_view) override {}
        void SetDebugName(Render::TextureViewHandle, std::string_view) override {}
        const Render::RenderFrameStatistics& GetFrameStatistics() const override
        {
            return statistics;
        }
        const std::vector<Render::RenderPassGpuTiming>& GetPassGpuTimings() const override
        {
            return timings;
        }
        Render::IRHICommandList* AllocateCommandList() override
        {
            return new MinimalCommandList();
        }
        void Submit(Render::IRHICommandList* commandList) override
        {
            delete commandList;
        }
        void DestroyBuffer(Render::BufferHandle) override {}
        void DestroyTexture(Render::TextureHandle) override {}
        void DestroyShader(Render::ShaderHandle) override {}
        void DestroyPipeline(Render::PipelineHandle) override {}
        void DestroySampler(Render::SamplerHandle) override {}
        void DestroyTextureView(Render::TextureViewHandle) override {}
        Render::TextureHandle GetActiveSwapchainTexture() override
        {
            return {};
        }
        void WaitIdle() override {}
        void Resize(uint32_t, uint32_t) override {}

        uint32_t nextHandle = 1;
        Render::RenderFrameStatistics statistics;
        std::vector<Render::RenderPassGpuTiming> timings;
    };

    /** @brief Creates a deterministic static opaque snapshot with mixed groups and visibility. */
    std::shared_ptr<const Render::RenderWorldSnapshot> BuildSnapshot()
    {
        auto snapshot = std::make_shared<Render::RenderWorldSnapshot>();
        snapshot->objects.reserve(8);
        for (uint32_t index = 0; index < 8; ++index)
        {
            Render::RenderObjectProxy proxy;
            proxy.mesh = Resource::MeshHandle::FromParts(index % 2u, 1);
            proxy.material = Resource::MaterialHandle::FromParts(index % 3u, 1);
            proxy.flags = Render::RenderObjectFlags::Visible | Render::RenderObjectFlags::CastShadow;
            proxy.layerMask = index == 3u ? 0u : 0xFFFFFFFFu;
            proxy.bounds.valid = false;
            if (index == 6u)
                proxy.flags = proxy.flags | Render::RenderObjectFlags::Transparent;
            if (index == 7u)
                proxy.flags = proxy.flags | Render::RenderObjectFlags::Skinned;

            snapshot->objects.push_back({
                .handle = Render::RenderObjectHandle::FromParts(index + 10u, 1),
                .proxy = proxy,
            });
        }
        return snapshot;
    }

    /** @brief Builds copied resource metadata without touching ResourceManager or RHI. */
    Render::RenderResourceView BuildResourceView()
    {
        std::vector<Render::RenderMeshMetadata> meshes{
            { .handle = Resource::MeshHandle::FromParts(0, 1), .indexCount = 36 },
            { .handle = Resource::MeshHandle::FromParts(1, 1), .indexCount = 128 },
        };
        std::vector<Render::RenderMaterialMetadata> materials;
        for (uint32_t index = 0; index < 3; ++index)
        {
            materials.push_back({
                .handle = Resource::MaterialHandle::FromParts(index, 1),
                .forwardPipeline = Render::PipelineHandle::FromParts(index + 20u, 1),
                .gbufferPipeline = Render::PipelineHandle::FromParts(index + 40u, 1),
                .shadowPipeline = Render::PipelineHandle::FromParts(index + 60u, 1),
            });
        }
        return Render::RenderResourceView(std::move(meshes), std::move(materials));
    }

    /** @brief Verifies capability selection never silently claims GPU when a requirement is missing. */
    void TestCapabilityFallback()
    {
        Render::RHICapabilities capable{
            .compute = true,
            .storageBuffer = true,
            .drawIndirect = true,
            .drawIndexedIndirect = true,
        };
        const Render::RenderPathSelection missingConsumer = Render::SelectRenderPath(capable,
                                                                                     {
                                                                                         .requested = Render::RenderPathMode::GpuDriven,
                                                                                         .jobSystemAvailable = true,
                                                                                         .gpuDrivenConsumerAvailable = false,
                                                                                     });
        Check(missingConsumer.effective == Render::RenderPathMode::JobCpu, "GPU request falls back to jobs when the material consumer is unavailable");
        Check(missingConsumer.fallback == Render::RenderPathFallbackReason::MissingGpuDrivenConsumer, "GPU consumer fallback is diagnostic");

        capable.compute = false;
        const auto missingCompute = Render::SelectRenderPath(capable, { .requested = Render::RenderPathMode::GpuDriven, .jobSystemAvailable = false, .gpuDrivenConsumerAvailable = true });
        Check(missingCompute.effective == Render::RenderPathMode::SerialCpu, "GPU request falls back to serial when jobs are unavailable");
        Check(missingCompute.fallback == Render::RenderPathFallbackReason::MissingCompute, "missing compute reports the root capability");

        const auto strict = Render::SelectRenderPath(capable,
                                                     {
                                                         .requested = Render::RenderPathMode::GpuDriven,
                                                         .jobSystemAvailable = true,
                                                         .strictGpuDriven = true,
                                                         .gpuDrivenConsumerAvailable = true,
                                                     });
        Check(strict.strictFailure, "strict GPU mode reports failure instead of silent fallback");
    }

    /** @brief Verifies ABI constants and deterministic static opaque draw-group generation. */
    void TestGpuDrivenLayoutAndBuild()
    {
        Check(alignof(Render::GpuInstanceData) == 16, "GPU instance data is std430-friendly");
        Check(sizeof(Render::GpuDrawGroup) % 16 == 0, "GPU draw group size is 16-byte aligned");
        Check(offsetof(Render::GpuInstanceData, boundsCenterRadius) == sizeof(float) * 16u, "bounds center follows model matrix");
        Check(Render::ComputeGpuDrivenLayoutHash() != 0, "GPU layout hash is non-zero");

        const auto snapshot = BuildSnapshot();
        const Render::RenderSceneView scene = Render::RenderSceneView::Build(snapshot);
        const Render::RenderView view{ .layerMask = 0xFFFFFFFFu, .primary = true };
        const Render::RenderResourceView resources = BuildResourceView();
        const Render::GpuDrivenFrameData first = Render::BuildGpuDrivenFrameData(scene, view, resources);
        const Render::GpuDrivenFrameData second = Render::BuildGpuDrivenFrameData(scene, view, resources);

        Check(first.inputStaticOpaqueCount == 6, "GPU-driven input excludes transparent and skinned objects");
        Check(first.cpuVisibleCount == 5 && first.cpuCulledCount == 1, "CPU visibility oracle respects layer visibility");
        Check(first.drawGroups.size() == first.indirectCommands.size(), "one indirect command is generated per draw group");
        Check(first.visibilityHash == second.visibilityHash && first.indirectHash == second.indirectHash, "GPU-driven hashes are deterministic");

        auto reorderedSnapshot = std::make_shared<Render::RenderWorldSnapshot>(*snapshot);
        std::ranges::reverse(reorderedSnapshot->objects);
        const Render::RenderSceneView reorderedScene = Render::RenderSceneView::Build(reorderedSnapshot);
        const Render::GpuDrivenFrameData reordered = Render::BuildGpuDrivenFrameData(reorderedScene, view, resources);
        Check(first.visibilityHash == reordered.visibilityHash, "GPU-driven visibility hash is source-order independent");
        Check(first.indirectHash == reordered.indirectHash, "GPU-driven indirect hash is source-order independent");
    }

    /** @brief Verifies GPU-driven RenderGraph pass order and optional consumer culling behavior. */
    void TestRenderGraphFlow()
    {
        MinimalDevice device;
        Render::RenderGraph graph(&device);
        Render::RenderGraphBlackboard blackboard;
        const Render::GpuDrivenGraphHandles handles = Render::AddGpuDrivenBufferFlow(graph,
                                                                                    blackboard,
                                                                                    {
                                                                                        .instanceCapacity = 128,
                                                                                        .drawGroupCapacity = 8,
                                                                                        .visibleCapacity = 128,
                                                                                        .addGraphicsConsumer = true,
                                                                                    });
        Check(handles.instanceData.IsValid() && blackboard.GetBuffer(Render::RenderGraphSemantic::GpuIndirectArgs).IsValid(), "GPU-driven buffers are published to the blackboard");
        const bool compiled = graph.Compile();
        if (!compiled)
        {
            for (const std::string& error : graph.GetCompileErrors())
                std::cerr << "RenderGraph error: " << error << '\n';
        }
        Check(compiled, "GPU-driven graph compiles with consumer");
        Check(graph.GetCompiledPassNames() == std::vector<std::string>({ "GPU Driven Upload", "GPU Driven Reset", "GPU Driven Cull", "GPU Driven Graphics Consumer" }), "GPU-driven graph preserves upload-reset-cull-consume order");

        Render::RenderGraph culledGraph(&device);
        Render::RenderGraphBlackboard culledBlackboard;
        Render::AddGpuDrivenBufferFlow(culledGraph, culledBlackboard, { .instanceCapacity = 128, .drawGroupCapacity = 8, .visibleCapacity = 128, .addGraphicsConsumer = false });
        const bool culledCompiled = culledGraph.Compile();
        if (!culledCompiled)
        {
            for (const std::string& error : culledGraph.GetCompileErrors())
                std::cerr << "RenderGraph error: " << error << '\n';
        }
        Check(culledCompiled, "GPU-driven graph without consumer still compiles");
        Check(culledGraph.GetCompiledPassNames().empty(), "GPU-driven producer passes are culled when no consumer exists");
    }

    /** @brief Verifies delayed readback matching and deterministic visibility diff output. */
    void TestValidationDiff()
    {
        const Render::GpuVisibilityDiff diff = Render::CompareVisibilitySets({ 1, 2, 3, 3, 5 }, { 2, 3, 4 });
        Check(!diff.matches, "visibility diff detects mismatch");
        Check(diff.missingFromGpu == std::vector<uint32_t>({ 1, 5 }), "visibility diff reports missing IDs sorted");
        Check(diff.extraOnGpu == std::vector<uint32_t>({ 4 }), "visibility diff reports extra IDs sorted");

        Render::GpuVisibilityValidationQueue queue;
        queue.EnqueueCpuOracle(10, { 1, 2, 3 });
        queue.EnqueueCpuOracle(11, { 7 });
        const Render::GpuVisibilityDiff matched = queue.ConsumeReadback({ .frameId = 10, .visibleObjectIds = { 1, 2, 3 }, .valid = true });
        Check(matched.matches, "readback is matched to the original frame oracle");
        Check(queue.PendingCount() == 1, "consuming one readback keeps later frames pending");
        queue.Clear();
        Check(queue.PendingCount() == 0, "validation queue clears on scene or mode changes");
    }

    /** @brief Verifies indirect submission obeys RHI capability limits before recording commands. */
    void TestIndirectSubmission()
    {
        MinimalCommandList commandList;
        Render::RHICapabilities capabilities{
            .drawIndirect = true,
            .drawIndexedIndirect = true,
            .multiDrawIndirect = false,
        };
        const Render::BufferHandle indirectBuffer = Render::BufferHandle::FromParts(8, 1);
        Check(Render::RecordGpuDrivenIndexedIndirect(commandList, capabilities, { .indirectArguments = indirectBuffer, .commandCount = 1 }), "single indirect command records without multi-draw support");
        Check(commandList.indexedIndirectCalls == 1 && commandList.lastStride == sizeof(Render::GpuIndexedIndirectCommand), "indirect submission uses the shared argument stride");
        Check(!Render::RecordGpuDrivenIndexedIndirect(commandList, capabilities, { .indirectArguments = indirectBuffer, .commandCount = 2 }), "multi indirect command is rejected without multiDrawIndirect");

        capabilities.multiDrawIndirect = true;
        Check(Render::RecordGpuDrivenIndexedIndirect(commandList, capabilities, { .indirectArguments = indirectBuffer, .offset = 20, .commandCount = 2 }), "multi indirect command records when capability is enabled");
        Check(commandList.lastOffset == 20 && commandList.lastDrawCount == 2, "indirect submission forwards offset and command count");
    }
} // namespace

/** @brief Runs the GPU-driven Phase 4 contract tests without requiring a real Vulkan device. */
int main()
{
    TestCapabilityFallback();
    TestGpuDrivenLayoutAndBuild();
    TestRenderGraphFlow();
    TestValidationDiff();
    TestIndirectSubmission();
    return g_failures == 0 ? 0 : 1;
}
