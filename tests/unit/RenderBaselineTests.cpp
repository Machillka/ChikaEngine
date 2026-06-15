#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RenderDebugVisualization.hpp"
#include "ChikaEngine/RenderDiagnostics.hpp"
#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/RenderQueue.hpp"
#include "ChikaEngine/RenderPipeline.hpp"
#include "ChikaEngine/RenderPipelinePasses.hpp"
#include "ChikaEngine/RenderVisibility.hpp"
#include "ChikaEngine/RenderWorld.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/debug/Gizmo.hpp"
#include "ChikaEngine/math/Bounds.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    namespace Shader = ChikaEngine::Shader;

    int g_failures = 0;

    /**
     * @brief 记录测试断言失败，同时继续执行后续检查以提供完整回归信息。
     */
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    class MockRHIDevice;

    /**
     * @brief 为 RenderGraph 单元测试提供不依赖 GPU 的命令列表。
     *
     * Mock 只记录顺序和统计，不模拟 Vulkan 行为；这样测试可以稳定验证上层调度契约。
     */
    class MockCommandList final : public ChikaEngine::Render::IRHICommandList
    {
      public:
        explicit MockCommandList(MockRHIDevice& device) : m_device(device) {}

        void Begin() override {}
        void End() override {}
        void SetDebugName(std::string_view name) override;
        void BeginDebugLabel(std::string_view name, const float color[4]) override;
        void EndDebugLabel() override {}
        void BeginTimestampScope(std::string_view) override {}
        void EndTimestampScope() override {}
        void BeginRendering(const std::vector<ChikaEngine::Render::RenderingAttachment>& colors, const ChikaEngine::Render::RenderingAttachment* depth) override;
        void EndRendering() override {}
        void InsertTextureBarrier(ChikaEngine::Render::TextureHandle, ChikaEngine::Render::ResourceState, ChikaEngine::Render::ResourceState, const ChikaEngine::Render::TextureSubresourceRange&) override;
        void InsertBufferBarrier(ChikaEngine::Render::BufferHandle, ChikaEngine::Render::ResourceState, ChikaEngine::Render::ResourceState, const ChikaEngine::Render::BufferRange&) override;
        void BindPipeline(ChikaEngine::Render::PipelineHandle) override;
        void BindResources(const ChikaEngine::Render::ResourceBindingGroup& group) override;
        void BindVertexBuffer(ChikaEngine::Render::BufferHandle, uint64_t) override {}
        void BindIndexBuffer(ChikaEngine::Render::BufferHandle, uint64_t, bool) override {}
        void PushConstants(std::string_view, const void*, uint32_t) override {}
        void CopyBuffer(ChikaEngine::Render::BufferHandle, ChikaEngine::Render::BufferHandle, uint64_t) override {}
        void CopyBufferToTexture(ChikaEngine::Render::BufferHandle, ChikaEngine::Render::TextureHandle, uint32_t, uint32_t) override {}
        void Draw(uint32_t, uint32_t instanceCount, uint32_t, uint32_t) override;
        void DrawIndexed(uint32_t, uint32_t instanceCount, uint32_t, int32_t, uint32_t) override;
        void DrawIndirect(ChikaEngine::Render::BufferHandle, uint64_t, uint32_t drawCount, uint32_t) override;
        void DrawIndexedIndirect(ChikaEngine::Render::BufferHandle, uint64_t, uint32_t drawCount, uint32_t) override;
        void Dispatch(uint32_t, uint32_t, uint32_t) override;

      private:
        MockRHIDevice& m_device;
    };

    /**
     * @brief 为 RenderGraph 提供最小 RHI 设备并保存测试可观察数据。
     *
     * 使用递增 Handle 模拟资源身份，避免测试依赖真实 Vulkan 实例、窗口或显卡驱动。
     */
    class MockRHIDevice final : public ChikaEngine::Render::IRHIDevice
    {
      public:
        void Initialize(const ChikaEngine::Render::RHI_InitParams&) override {}
        void Shutdown() override {}
        void BeginFrame() override
        {
            statistics.Reset();
        }
        void EndFrame() override {}

        ChikaEngine::Render::BufferHandle CreateBuffer(const ChikaEngine::Render::BufferDesc&) override
        {
            return ChikaEngine::Render::BufferHandle::FromParts(nextHandle++, 1);
        }
        ChikaEngine::Render::TextureHandle CreateTexture(const ChikaEngine::Render::TextureDesc&) override
        {
            return ChikaEngine::Render::TextureHandle::FromParts(nextHandle++, 1);
        }
        ChikaEngine::Render::SamplerHandle CreateSampler(const ChikaEngine::Render::SamplerDesc&) override
        {
            return ChikaEngine::Render::SamplerHandle::FromParts(nextHandle++, 1);
        }
        ChikaEngine::Render::TextureViewHandle CreateTextureView(const ChikaEngine::Render::TextureViewDesc&) override
        {
            return ChikaEngine::Render::TextureViewHandle::FromParts(nextHandle++, 1);
        }
        ChikaEngine::Render::ShaderHandle CreateShader(const ChikaEngine::Render::ShaderDesc&) override
        {
            return ChikaEngine::Render::ShaderHandle::FromParts(nextHandle++, 1);
        }
        ChikaEngine::Render::PipelineHandle CreateGraphicsPipeline(const ChikaEngine::Render::PipelineDesc&) override
        {
            return ChikaEngine::Render::PipelineHandle::FromParts(nextHandle++, 1);
        }
        ChikaEngine::Render::PipelineHandle CreateComputePipeline(const ChikaEngine::Render::ComputePipelineDesc&) override
        {
            return ChikaEngine::Render::PipelineHandle::FromParts(nextHandle++, 1);
        }
        void* GetMappedData(ChikaEngine::Render::BufferHandle) override
        {
            return nullptr;
        }

        void SetDebugName(ChikaEngine::Render::BufferHandle, std::string_view name) override
        {
            resourceNames.emplace_back(name);
        }
        void SetDebugName(ChikaEngine::Render::TextureHandle, std::string_view name) override
        {
            resourceNames.emplace_back(name);
        }
        void SetDebugName(ChikaEngine::Render::ShaderHandle, std::string_view name) override
        {
            resourceNames.emplace_back(name);
        }
        void SetDebugName(ChikaEngine::Render::PipelineHandle, std::string_view name) override
        {
            resourceNames.emplace_back(name);
        }
        void SetDebugName(ChikaEngine::Render::SamplerHandle, std::string_view name) override
        {
            resourceNames.emplace_back(name);
        }
        void SetDebugName(ChikaEngine::Render::TextureViewHandle, std::string_view name) override
        {
            resourceNames.emplace_back(name);
        }
        const ChikaEngine::Render::RenderFrameStatistics& GetFrameStatistics() const override
        {
            return statistics;
        }
        const std::vector<ChikaEngine::Render::RenderPassGpuTiming>& GetPassGpuTimings() const override
        {
            return gpuTimings;
        }

        ChikaEngine::Render::IRHICommandList* AllocateCommandList() override
        {
            return new MockCommandList(*this);
        }
        void Submit(ChikaEngine::Render::IRHICommandList* commandList) override
        {
            submittedCommandList.reset(commandList);
        }
        void DestroyBuffer(ChikaEngine::Render::BufferHandle) override {}
        void DestroyTexture(ChikaEngine::Render::TextureHandle) override {}
        void DestroyShader(ChikaEngine::Render::ShaderHandle) override {}
        void DestroyPipeline(ChikaEngine::Render::PipelineHandle) override {}
        void DestroySampler(ChikaEngine::Render::SamplerHandle) override {}
        void DestroyTextureView(ChikaEngine::Render::TextureViewHandle) override {}
        ChikaEngine::Render::TextureHandle GetActiveSwapchainTexture() override
        {
            return {};
        }
        void WaitIdle() override {}
        void Resize(uint32_t, uint32_t) override {}

        uint32_t nextHandle = 0;
        ChikaEngine::Render::RenderFrameStatistics statistics;
        std::vector<ChikaEngine::Render::RenderPassGpuTiming> gpuTimings;
        std::string commandListName;
        std::vector<std::string> labels;
        std::vector<std::string> resourceNames;
        uint32_t textureBarrierCount = 0;
        uint32_t bufferBarrierCount = 0;
        uint32_t dispatchCount = 0;
        std::vector<uint32_t> renderingColorCounts;
        std::vector<bool> renderingHasDepth;
        std::unique_ptr<ChikaEngine::Render::IRHICommandList> submittedCommandList;
    };

    /**
     * @brief 保存 Mock Command List 名称，验证 RenderGraph 会为整帧命令列表提供稳定名称。
     */
    void MockCommandList::SetDebugName(std::string_view name)
    {
        m_device.commandListName = name;
    }

    /**
     * @brief 保存 Pass 标签顺序，验证编译顺序与实际执行顺序一致。
     */
    void MockCommandList::BeginDebugLabel(std::string_view name, const float[4])
    {
        m_device.labels.emplace_back(name);
    }

    /**
     * @brief 保存 Graphics Pass Attachment 形状，用于验证 Depth-only Pass。
     */
    void MockCommandList::BeginRendering(const std::vector<ChikaEngine::Render::RenderingAttachment>& colors, const ChikaEngine::Render::RenderingAttachment* depth)
    {
        m_device.renderingColorCounts.push_back(static_cast<uint32_t>(colors.size()));
        m_device.renderingHasDepth.push_back(depth != nullptr);
    }

    void MockCommandList::InsertTextureBarrier(ChikaEngine::Render::TextureHandle, ChikaEngine::Render::ResourceState, ChikaEngine::Render::ResourceState, const ChikaEngine::Render::TextureSubresourceRange&)
    {
        ++m_device.textureBarrierCount;
    }

    void MockCommandList::InsertBufferBarrier(ChikaEngine::Render::BufferHandle, ChikaEngine::Render::ResourceState, ChikaEngine::Render::ResourceState, const ChikaEngine::Render::BufferRange&)
    {
        ++m_device.bufferBarrierCount;
    }

    /**
     * @brief 模拟 Pipeline Bind 统计，用于验证统计结构可以由 RHI 命令层累加。
     */
    void MockCommandList::BindPipeline(ChikaEngine::Render::PipelineHandle)
    {
        ++m_device.statistics.pipelineBindCount;
    }

    /**
     * @brief 按实际绑定资源数量模拟 Descriptor Write 统计。
     */
    void MockCommandList::BindResources(const ChikaEngine::Render::ResourceBindingGroup& group)
    {
        m_device.statistics.descriptorUpdateCount += static_cast<uint32_t>(group.buffers.size() + group.textures.size() + group.samplers.size());
    }

    /**
     * @brief 将间接绘制按 drawCount 计入 Mock 统计。
     */
    void MockCommandList::DrawIndirect(ChikaEngine::Render::BufferHandle, uint64_t, uint32_t drawCount, uint32_t)
    {
        m_device.statistics.drawCallCount += drawCount;
    }

    /**
     * @brief 将索引间接绘制按 drawCount 计入 Mock 统计。
     */
    void MockCommandList::DrawIndexedIndirect(ChikaEngine::Render::BufferHandle, uint64_t, uint32_t drawCount, uint32_t)
    {
        m_device.statistics.drawCallCount += drawCount;
    }

    void MockCommandList::Dispatch(uint32_t, uint32_t, uint32_t)
    {
        ++m_device.dispatchCount;
    }

    /**
     * @brief 模拟非索引 Draw，并分别累计 Draw Call 与 Instance。
     */
    void MockCommandList::Draw(uint32_t, uint32_t instanceCount, uint32_t, uint32_t)
    {
        ++m_device.statistics.drawCallCount;
        m_device.statistics.instanceCount += instanceCount;
    }

    /**
     * @brief 模拟索引 Draw，保持与非索引 Draw 相同的统计契约。
     */
    void MockCommandList::DrawIndexed(uint32_t, uint32_t instanceCount, uint32_t, int32_t, uint32_t)
    {
        Draw(0, instanceCount, 0, 0);
    }

    /**
     * @brief 验证帧统计可以可靠清零，防止跨帧数据污染性能基线。
     */
    void TestFrameStatisticsReset()
    {
        ChikaEngine::Render::RenderFrameStatistics statistics{
            .passCount = 3,
            .drawCallCount = 4,
            .instanceCount = 5,
            .pipelineBindCount = 6,
            .descriptorUpdateCount = 7,
        };
        statistics.Reset();

        Check(statistics.passCount == 0, "frame statistics reset pass count");
        Check(statistics.drawCallCount == 0, "frame statistics reset draw count");
        Check(statistics.instanceCount == 0, "frame statistics reset instance count");
        Check(statistics.pipelineBindCount == 0, "frame statistics reset pipeline bind count");
        Check(statistics.descriptorUpdateCount == 0, "frame statistics reset descriptor update count");
        Check(statistics.visibleObjectCount == 0 && statistics.batchCount == 0, "frame statistics reset visibility and batch counts");
    }

    /**
     * @brief 验证 Phase 5 帧级 GPU 数据布局和质量设置具有稳定默认契约。
     */
    void TestPhase5RenderFeatureContracts()
    {
        using namespace ChikaEngine::Render;

        const RenderSettings settings;
        Check(sizeof(SceneData) == 176, "scene data matches shader std140 layout");
        Check(sizeof(LightGPU) == 64, "light data matches shader std430 layout");
        Check(sizeof(PostProcessData) == 32, "post process data matches shader std140 layout");
        Check(settings.postProcess.toneMappingEnabled && settings.postProcess.fxaaEnabled, "HDR output defaults to tone mapping and FXAA");
        Check(settings.shadows.resolution == 2048 && settings.shadows.pcfRadius == 1, "shadow quality defaults are explicit");
        Check(RHI_Format::RGBA8_SRGB != RHI_Format::RGBA8_UNorm, "sRGB and linear texture formats are distinct");
    }

    /**
     * @brief 验证 RenderWorld 稳定句柄、增量更新和不可变 Snapshot 边界。
     */
    void TestRenderWorldLifecycleAndSnapshot()
    {
        using namespace ChikaEngine::Render;

        RenderWorld world;
        RenderObjectProxy proxy;
        proxy.mesh = ChikaEngine::Resource::MeshHandle::FromParts(2, 1);
        proxy.material = ChikaEngine::Resource::MaterialHandle::FromParts(3, 1);

        const RenderObjectHandle object = world.CreateObject(proxy);
        const uint64_t createdRevision = world.GetRevision();
        Check(object.IsValid() && world.GetObjectCount() == 1, "render world creates stable object proxy");
        Check(!world.UpdateObject(object, proxy) && world.GetRevision() == createdRevision, "unchanged proxy does not dirty render world");

        proxy.transform(0, 3) = 4.0f;
        Check(world.UpdateObject(object, proxy), "changed proxy updates render world");

        RenderView primaryView{ .primary = true };
        world.CreateView(primaryView);
        world.CreateLight({});
        world.CreateLight({ .type = RenderLightType::Point, .position = { 2.0f, 3.0f, 4.0f }, .color = { 1.0f, 0.2f, 0.1f }, .intensity = 4.0f, .range = 8.0f, .castsShadow = false });
        world.CreateLight({ .type = RenderLightType::Spot, .innerConeCos = 0.95f, .outerConeCos = 0.75f, .castsShadow = false });
        const auto snapshot = world.CreateSnapshot();
        Check(snapshot->objects.size() == 1 && snapshot->lights.size() == 3, "render world snapshot copies objects and multiple light types");
        Check(snapshot->lights[2].proxy.outerConeCos == 0.75f, "spot light cone settings survive snapshot");
        Check(snapshot->viewFamily.GetPrimaryView() != nullptr, "view family exposes primary view");

        Check(world.DestroyObject(object), "render world destroys proxy");
        Check(snapshot->objects.size() == 1, "existing snapshot remains immutable after world mutation");
        Check(world.GetObject(object) == nullptr, "stale render object handle is rejected");
    }

    /**
     * @brief 验证 Local Bounds 在平移、旋转和非均匀缩放后仍保守覆盖物体。
     */
    void TestBoundsTransform()
    {
        using namespace ChikaEngine::Math;

        const Bounds localBounds = Bounds::FromMinMax(Vector3(-1.0f, -2.0f, -3.0f), Vector3(1.0f, 2.0f, 3.0f));
        const Mat4 transform = Mat4::TRSMatrix(Vector3(10.0f, 20.0f, 30.0f), Quaternion::Identity(), Vector3(2.0f, 3.0f, 4.0f));
        const Bounds worldBounds = TransformBounds(localBounds, transform);

        Check(worldBounds.valid, "transformed bounds remain valid");
        Check(worldBounds.center == Vector3(10.0f, 20.0f, 30.0f), "bounds center follows world translation");
        Check(worldBounds.extents == Vector3(2.0f, 6.0f, 12.0f), "bounds extents include non-uniform world scale");
        Check(ExpandBounds(localBounds, 1.25f).extents == Vector3(1.25f, 2.5f, 3.75f), "conservative bounds expansion preserves center");
    }

    /**
     * @brief 验证 Visibility 在 Queue 构建前剔除视锥外、隐藏和 Layer 不匹配对象。
     */
    void TestFrustumVisibility()
    {
        using namespace ChikaEngine::Render;

        RenderWorldSnapshot snapshot;
        RenderObjectProxy inside;
        inside.bounds = { .center = { 0.0f, 0.0f, 0.5f }, .extents = { 0.1f, 0.1f, 0.1f }, .sphereRadius = 0.2f, .valid = true };
        RenderObjectProxy outside = inside;
        outside.bounds.center.x = 5.0f;
        RenderObjectProxy hidden = inside;
        hidden.flags = RenderObjectFlags::CastShadow;
        snapshot.objects = {
            { RenderObjectHandle::FromParts(1, 1), inside },
            { RenderObjectHandle::FromParts(2, 1), outside },
            { RenderObjectHandle::FromParts(3, 1), hidden },
        };

        const VisibilityResult visibility = BuildVisibility(snapshot, RenderView{ .viewProjection = ChikaEngine::Math::Mat4::Identity() });
        Check(visibility.testedObjectCount == 3, "visibility tests every render proxy once");
        Check(visibility.visibleObjectCount == 1 && visibility.culledObjectCount == 2, "frustum and flags reject invisible proxies");
        Check(visibility.visibleObjects.front()->handle == snapshot.objects.front().handle, "visibility retains stable render object identity");
    }

    /**
     * @brief 验证 Opaque 稳定排序会把相同状态 Packet 合并成可实例化 Batch。
     */
    void TestRenderQueueSortAndBatch()
    {
        using namespace ChikaEngine::Render;

        std::vector<RenderObjectSnapshot> objects(3);
        for (uint32_t index = 0; index < objects.size(); ++index)
            objects[index].handle = RenderObjectHandle::FromParts(index + 1, 1);

        RenderQueue queue;
        queue.packets = {
            { .object = &objects[2], .pass = RenderPassClass::ForwardOpaque, .pipeline = PipelineHandle::FromParts(2, 1), .material = ChikaEngine::Resource::MaterialHandle::FromParts(2, 1), .mesh = ChikaEngine::Resource::MeshHandle::FromParts(2, 1), .instancingEligible = true },
            { .object = &objects[1], .pass = RenderPassClass::ForwardOpaque, .pipeline = PipelineHandle::FromParts(1, 1), .material = ChikaEngine::Resource::MaterialHandle::FromParts(1, 1), .mesh = ChikaEngine::Resource::MeshHandle::FromParts(1, 1), .instancingEligible = true },
            { .object = &objects[0], .pass = RenderPassClass::ForwardOpaque, .pipeline = PipelineHandle::FromParts(1, 1), .material = ChikaEngine::Resource::MaterialHandle::FromParts(1, 1), .mesh = ChikaEngine::Resource::MeshHandle::FromParts(1, 1), .instancingEligible = true },
        };

        SortAndBuildRenderBatches(queue, false);
        Check(queue.packets.front().pipeline == PipelineHandle::FromParts(1, 1), "opaque sort groups lower pipeline state first");
        Check(queue.batches.size() == 2, "batch builder separates different render states");
        Check(queue.batches.front().packetCount == 2 && queue.batches.front().instanced, "matching static packets form one instanced batch");

        RenderQueue transparentQueue;
        transparentQueue.packets = {
            { .object = &objects[0], .pass = RenderPassClass::ForwardTransparent, .viewDepth = 2.0f },
            { .object = &objects[1], .pass = RenderPassClass::ForwardTransparent, .viewDepth = 8.0f },
        };
        SortAndBuildRenderBatches(transparentQueue, true);
        Check(transparentQueue.packets.front().viewDepth == 8.0f, "transparent sort records farther packets before nearer packets");
        Check(transparentQueue.batches.size() == 2, "transparent packets remain separate draw batches");

        SortAndBuildRenderBatches(queue, false);
        Check(queue.batches.size() == 2, "rebuilding a render queue replaces stale batches");
    }

    /**
     * @brief 验证 RenderWorld 调试可视化生成 AABB 与 View/Light Frustum，且颜色反映剔除结果。
     */
    void TestRenderDebugVisualization()
    {
        using namespace ChikaEngine;

        Render::RenderWorldSnapshot snapshot;
        Render::RenderObjectProxy visible;
        visible.bounds = { .center = { 0.0f, 0.0f, 0.5f }, .extents = { 0.1f, 0.1f, 0.1f }, .sphereRadius = 0.2f, .valid = true };
        Render::RenderObjectProxy culled = visible;
        culled.bounds.center.x = 5.0f;
        snapshot.objects = {
            { Render::RenderObjectHandle::FromParts(1, 1), visible },
            { Render::RenderObjectHandle::FromParts(2, 1), culled },
        };
        snapshot.viewFamily.views.push_back({ Render::RenderViewHandle::FromParts(1, 1), { .viewProjection = Math::Mat4::Identity(), .primary = true } });
        snapshot.lights.push_back({ Render::RenderLightHandle::FromParts(1, 1), { .viewProjection = Math::Mat4::Identity() } });

        Debug::Gizmo::Clear();
        Render::AppendRenderWorldDebugGizmos(snapshot, { .drawAABBs = true, .drawFrustums = true });
        const auto& lines = Debug::Gizmo::GetLines();
        Check(Debug::Gizmo::GetLineCount() == 48, "debug visualization emits two AABBs and two frustums");
        Check(lines[0].color == Math::Vector4(0.1f, 1.0f, 0.2f, 1.0f), "visible AABB uses visible color");
        Check(lines[12].color == Math::Vector4(1.0f, 0.15f, 0.1f, 1.0f), "culled AABB uses culled color");

        Debug::Gizmo::Clear();
        Render::AppendRenderWorldDebugGizmos(snapshot, {});
        Check(Debug::Gizmo::GetLineCount() == 0, "disabled debug visualization emits no lines");
    }

    /**
     * @brief 验证 RHI 命令统计与资源命名契约，不依赖具体图形后端。
     */
    void TestRHIStatisticsAndNames()
    {
        using namespace ChikaEngine::Render;

        MockRHIDevice device;
        device.BeginFrame();

        const BufferHandle buffer = device.CreateBuffer({ .size = 64 });
        const TextureHandle texture = device.CreateTexture({ .width = 1, .height = 1 });
        const TextureViewHandle textureView = device.CreateTextureView({ .texture = texture });
        const SamplerHandle sampler = device.CreateSampler({});
        const PipelineHandle pipeline = device.CreateGraphicsPipeline({});
        device.SetDebugName(buffer, "Baseline.Buffer");
        device.SetDebugName(texture, "Baseline.Texture");
        device.SetDebugName(textureView, "Baseline.TextureView");
        device.SetDebugName(sampler, "Baseline.Sampler");
        device.SetDebugName(pipeline, "Baseline.Pipeline");

        const Shader::ShaderProgramInterface interface{
            .resources = {
                { .name = "Buffer", .set = 0, .binding = 0, .type = Shader::ShaderDescriptorType::UniformBuffer, .arrayCount = 1 },
                { .name = "Texture", .set = 0, .binding = 1, .type = Shader::ShaderDescriptorType::CombinedImageSampler, .arrayCount = 1 },
                { .name = "TextureArray", .set = 0, .binding = 2, .type = Shader::ShaderDescriptorType::SampledImage, .arrayCount = 2 },
                { .name = "Sampler", .set = 0, .binding = 3, .type = Shader::ShaderDescriptorType::Sampler, .arrayCount = 1 },
            },
        };
        const ResourceBindingHandle bufferBinding = ResolveResourceBinding(interface, "Buffer");
        const ResourceBindingHandle textureBinding = ResolveResourceBinding(interface, "Texture");
        const ResourceBindingHandle textureArrayBinding = ResolveResourceBinding(interface, "TextureArray");
        const ResourceBindingHandle samplerBinding = ResolveResourceBinding(interface, "Sampler");
        ResourceBindingGroup bindings{ .set = 0 };
        bindings.BindBuffer(bufferBinding, buffer, 0, 64);
        bindings.BindTexture(textureBinding, texture);
        bindings.BindTextureView(textureArrayBinding, textureView, 1);
        bindings.BindSampler(samplerBinding, sampler);

        Check(bufferBinding.IsValid(), "resource binding resolves once from shader interface");
        Check(!ResolveResourceBinding(interface, "Missing").IsValid(), "missing resource resolves to invalid binding");
        Check(!bindings.BindTexture(textureBinding, texture, 1), "resource binding rejects descriptor array overflow");
        IRHICommandList* commandList = device.AllocateCommandList();
        commandList->BindPipeline(pipeline);
        commandList->BindResources(bindings);
        commandList->Draw(3, 2);
        commandList->DrawIndexed(6, 3);
        device.Submit(commandList);

        const auto& statistics = device.GetFrameStatistics();
        Check(statistics.drawCallCount == 2, "RHI statistics count draw calls");
        Check(statistics.instanceCount == 5, "RHI statistics sum instances");
        Check(statistics.pipelineBindCount == 1, "RHI statistics count pipeline binds");
        Check(statistics.descriptorUpdateCount == 4, "RHI statistics count descriptor writes");
        Check(device.resourceNames == std::vector<std::string>({ "Baseline.Buffer", "Baseline.Texture", "Baseline.TextureView", "Baseline.Sampler", "Baseline.Pipeline" }), "RHI resources accept debug names");
    }

    /**
     * @brief 验证 RenderGraph 会剔除无用 Pass，并按依赖顺序执行和标记存活 Pass。
     */
    void TestRenderGraphCompileAndExecuteOrder()
    {
        using namespace ChikaEngine::Render;

        MockRHIDevice device;
        RenderGraph graph(&device);
        const TextureDesc colorDesc{
            .width = 64,
            .height = 64,
            .format = RHI_Format::RGBA8_UNorm,
            .usage = RHI_TextureUsage::ColorAttachment | RHI_TextureUsage::Sampled,
        };

        const RGTextureHandle swapchain = graph.ImportTexture("Swapchain", device.CreateTexture(colorDesc), colorDesc);
        const RGTextureHandle sceneColor = graph._RegisterTexture("SceneColor", colorDesc);
        const RGTextureHandle unusedColor = graph._RegisterTexture("UnusedColor", colorDesc);

        graph.AddPass("Scene", [&](RGPassBuilder& builder) { builder.WriteColor(sceneColor); }, [](IRHICommandList*, RenderGraph*) {});
        graph.AddPass("Unused", [&](RGPassBuilder& builder) { builder.WriteColor(unusedColor); }, [](IRHICommandList*, RenderGraph*) {});
        graph.AddPass(
            "Composite",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(sceneColor);
                builder.WriteColor(swapchain);
            },
            [](IRHICommandList*, RenderGraph*) {});
        graph.AddPresentPass("Present", swapchain);

        graph.Compile();
        const std::vector<std::string> compiledNames = graph.GetCompiledPassNames();
        Check(compiledNames == std::vector<std::string>({ "Scene", "Composite", "Present" }), "render graph compiles dependency order and culls unused pass");

        graph.Execute();
        Check(graph.GetLastExecutedPassCount() == 3, "render graph reports executed pass count");
        Check(device.commandListName == "RenderGraph.Frame", "render graph names frame command list");
        Check(device.labels == compiledNames, "render graph debug labels follow compiled pass order");
        Check(!graph.GetDebugSnapshot().dump.empty(), "render graph exposes a graph dump");
    }

    /**
     * @brief 验证 Copy/Compute/Graphics Buffer 依赖、Buffer Barrier 与 Dispatch 路径。
     */
    void TestRenderGraphBufferAndComputePath()
    {
        using namespace ChikaEngine::Render;

        MockRHIDevice device;
        RenderGraph graph(&device);
        const BufferDesc bufferDesc{
            .size = 256,
            .usage = RHI_BufferUsage::Storage | RHI_BufferUsage::TransferDst,
        };
        const TextureDesc colorDesc{
            .width = 16,
            .height = 16,
            .usage = RHI_TextureUsage::ColorAttachment,
        };
        const RGBufferHandle copied = graph._RegisterBuffer("Copied", bufferDesc);
        const RGBufferHandle computed = graph._RegisterBuffer("Computed", bufferDesc);
        const RGTextureHandle output = graph.ImportTexture("Output", device.CreateTexture(colorDesc), colorDesc);

        graph.AddCopyPass("Copy", [&](RGPassBuilder& builder) { builder.WriteBuffer(copied, ResourceState::CopyDst); }, [](IRHICommandList*, RenderGraph*) {});
        graph.AddComputePass(
            "Compute",
            [&](RGPassBuilder& builder)
            {
                builder.ReadBuffer(copied, ResourceState::StorageRead);
                builder.WriteBuffer(computed, ResourceState::StorageWrite);
            },
            [](IRHICommandList* commandList, RenderGraph*) { commandList->Dispatch(1, 1, 1); });
        graph.AddPass(
            "Graphics",
            [&](RGPassBuilder& builder)
            {
                builder.ReadBuffer(computed, ResourceState::StorageRead);
                builder.WriteColor(output);
            },
            [](IRHICommandList*, RenderGraph*) {});
        graph.AddPresentPass("Present", output);

        Check(graph.Compile(), "render graph compiles Copy/Compute/Graphics buffer chain");
        Check(graph.GetCompiledPassNames() == std::vector<std::string>({ "Copy", "Compute", "Graphics", "Present" }), "buffer dependencies preserve pass order");
        graph.Execute();
        Check(device.bufferBarrierCount >= 4, "render graph inserts buffer barriers");
        Check(device.dispatchCount == 1, "compute pass dispatches once");
    }

    /**
     * @brief 验证 Copy Pass 的普通 Texture 写入不会被误判为 Color Attachment。
     */
    void TestRenderGraphTextureCopyPath()
    {
        using namespace ChikaEngine::Render;

        MockRHIDevice device;
        RenderGraph graph(&device);
        const TextureDesc textureDesc{
            .width = 16,
            .height = 16,
            .usage = RHI_TextureUsage::Sampled,
        };
        const RGTextureHandle uploaded = graph.ImportTexture("UploadedTexture", device.CreateTexture(textureDesc), textureDesc, ResourceState::Undefined, ResourceState::ShaderResource);
        graph.AddCopyPass("Texture Upload", [&](RGPassBuilder& builder) { builder.WriteTexture(uploaded, ResourceState::CopyDst); }, [](IRHICommandList*, RenderGraph*) {});

        Check(graph.Compile(), "render graph accepts a Copy Pass writing a texture");
        graph.Execute();
        Check(device.renderingColorCounts.empty(), "texture copy does not begin graphics rendering");
        Check(device.textureBarrierCount == 2, "texture copy transitions to CopyDst and final ShaderResource");
    }

    /**
     * @brief 验证 Depth-only Pass 不创建 Color Attachment。
     */
    void TestRenderGraphDepthOnlyPass()
    {
        using namespace ChikaEngine::Render;

        MockRHIDevice device;
        RenderGraph graph(&device);
        const TextureDesc depthDesc{
            .width = 16,
            .height = 16,
            .format = RHI_Format::D32_SFloat,
            .usage = RHI_TextureUsage::DepthStencilAttachment | RHI_TextureUsage::Sampled,
        };
        const RGTextureHandle depth = graph._RegisterTexture("ShadowDepth", depthDesc);
        graph.AddPass("Shadow", [&](RGPassBuilder& builder) { builder.WriteDepth(depth); }, [](IRHICommandList*, RenderGraph*) {});
        graph.AddCopyPass("Consume", [&](RGPassBuilder& builder) { builder.ReadTexture(depth); }, [](IRHICommandList*, RenderGraph*) {});

        Check(graph.Compile(), "render graph compiles depth-only pass");
        graph.Execute();
        Check(!device.renderingColorCounts.empty() && device.renderingColorCounts.front() == 0 && device.renderingHasDepth.front(), "depth-only pass begins rendering without color attachment");
    }

    /**
     * @brief 验证非法 transient read-before-write 在 Compile 阶段失败。
     */
    void TestRenderGraphCompileValidation()
    {
        using namespace ChikaEngine::Render;

        MockRHIDevice device;
        RenderGraph graph(&device);
        const RGBufferHandle missingProducer = graph._RegisterBuffer("MissingProducer", { .size = 16, .usage = RHI_BufferUsage::Storage });
        graph.AddCopyPass("InvalidRead", [&](RGPassBuilder& builder) { builder.ReadBuffer(missingProducer, ResourceState::StorageRead); }, [](IRHICommandList*, RenderGraph*) {});
        Check(!graph.Compile(), "render graph rejects transient read-before-write");
        Check(!graph.GetCompileErrors().empty(), "render graph exposes compile errors");
    }

    /**
     * @brief 验证 HDR Scene Color 必须经过 Post Process 才能到达 LDR 输出和 Present。
     */
    void TestPostProcessGraphPath()
    {
        using namespace ChikaEngine::Render;

        MockRHIDevice device;
        RenderGraph graph(&device);
        RenderGraphBlackboard blackboard;
        const TextureDesc hdrDesc{
            .width = 16,
            .height = 16,
            .format = RHI_Format::RGBA16_Float,
            .usage = RHI_TextureUsage::ColorAttachment | RHI_TextureUsage::Sampled,
        };
        TextureDesc ldrDesc = hdrDesc;
        ldrDesc.format = RHI_Format::RGBA8_UNorm;
        const RGTextureHandle hdr = graph.ImportTexture("HDR", device.CreateTexture(hdrDesc), hdrDesc);
        const RGTextureHandle ldr = graph.ImportTexture("LDR", device.CreateTexture(ldrDesc), ldrDesc);
        blackboard.SetTexture(std::string(RenderGraphSemantic::HDRSceneColor), hdr);
        blackboard.SetTexture(std::string(RenderGraphSemantic::SceneColor), ldr);

        graph.AddPass("HDR Scene", [&](RGPassBuilder& builder) { builder.WriteColor(hdr); }, [](IRHICommandList*, RenderGraph*) {});
        PassModules::AddPostProcess(graph, blackboard, [](IRHICommandList* commandList, RenderGraph*) { commandList->Draw(3, 1); });
        graph.AddPresentPass("Present", ldr);

        Check(graph.Compile(), "post process graph compiles");
        Check(graph.GetCompiledPassNames() == std::vector<std::string>({ "HDR Scene", "Post Process Composite", "Present" }), "post process remains between HDR scene and LDR present");
    }
} // namespace

int main()
{
    TestFrameStatisticsReset();
    TestPhase5RenderFeatureContracts();
    TestRenderWorldLifecycleAndSnapshot();
    TestBoundsTransform();
    TestFrustumVisibility();
    TestRenderQueueSortAndBatch();
    TestRenderDebugVisualization();
    TestRHIStatisticsAndNames();
    TestRenderGraphCompileAndExecuteOrder();
    TestRenderGraphBufferAndComputePath();
    TestRenderGraphTextureCopyPath();
    TestRenderGraphDepthOnlyPass();
    TestRenderGraphCompileValidation();
    TestPostProcessGraphPath();

    if (g_failures != 0)
        std::cerr << g_failures << " render baseline test(s) failed\n";
    return g_failures == 0 ? 0 : 1;
}
