#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RenderDiagnostics.hpp"
#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/RenderWorld.hpp"
#include "ChikaEngine/Renderer.hpp"

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
        void BeginRendering(const std::vector<ChikaEngine::Render::RenderingAttachment>&, const ChikaEngine::Render::RenderingAttachment*) override {}
        void EndRendering() override {}
        void InsertTextureBarrier(ChikaEngine::Render::TextureHandle, ChikaEngine::Render::ResourceState, ChikaEngine::Render::ResourceState) override {}
        void BindPipeline(ChikaEngine::Render::PipelineHandle) override;
        void BindResources(const ChikaEngine::Render::ResourceBindingGroup& group) override;
        void BindVertexBuffer(ChikaEngine::Render::BufferHandle, uint64_t) override {}
        void BindIndexBuffer(ChikaEngine::Render::BufferHandle, uint64_t, bool) override {}
        void PushConstants(std::string_view, const void*, uint32_t) override {}
        void CopyBuffer(ChikaEngine::Render::BufferHandle, ChikaEngine::Render::BufferHandle, uint64_t) override {}
        void CopyBufferToTexture(ChikaEngine::Render::BufferHandle, ChikaEngine::Render::TextureHandle, uint32_t, uint32_t) override {}
        void Draw(uint32_t, uint32_t instanceCount) override;
        void DrawIndexed(uint32_t, uint32_t instanceCount) override;
        void DrawImGui(void*) override {}

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
        ChikaEngine::Render::ShaderHandle CreateShader(const ChikaEngine::Render::ShaderDesc&) override
        {
            return ChikaEngine::Render::ShaderHandle::FromParts(nextHandle++, 1);
        }
        ChikaEngine::Render::PipelineHandle CreateGraphicsPipeline(const ChikaEngine::Render::PipelineDesc&) override
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
        const ChikaEngine::Render::RenderFrameStatistics& GetFrameStatistics() const override
        {
            return statistics;
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
        ChikaEngine::Render::TextureHandle GetActiveSwapchainTexture() override
        {
            return {};
        }
        void InitializeImgui() override {}
        void* GetImGuiTextureHandle(ChikaEngine::Render::TextureHandle) override
        {
            return nullptr;
        }
        void WaitIdle() override {}
        void Resize(uint32_t, uint32_t) override {}

        uint32_t nextHandle = 0;
        ChikaEngine::Render::RenderFrameStatistics statistics;
        std::string commandListName;
        std::vector<std::string> labels;
        std::vector<std::string> resourceNames;
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
     * @brief 模拟非索引 Draw，并分别累计 Draw Call 与 Instance。
     */
    void MockCommandList::Draw(uint32_t, uint32_t instanceCount)
    {
        ++m_device.statistics.drawCallCount;
        m_device.statistics.instanceCount += instanceCount;
    }

    /**
     * @brief 模拟索引 Draw，保持与非索引 Draw 相同的统计契约。
     */
    void MockCommandList::DrawIndexed(uint32_t, uint32_t instanceCount)
    {
        Draw(0, instanceCount);
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
    }

    /**
     * @brief 验证当前 DrawCommand 基础契约，作为后续 RenderWorld 迁移前的行为冻结点。
     */
    void TestDrawCommandDefaults()
    {
        ChikaEngine::Render::DrawCommand command{};
        Check(!command.meshHandle.IsValid(), "draw command mesh handle defaults invalid");
        Check(!command.materialHandle.IsValid(), "draw command material handle defaults invalid");
        Check(!command.boneUBO.IsValid(), "draw command bone buffer defaults invalid");
        Check(!command.isSkinned, "draw command defaults to static mesh");
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
        const auto snapshot = world.CreateSnapshot();
        Check(snapshot->objects.size() == 1 && snapshot->lights.size() == 1, "render world snapshot copies objects and lights");
        Check(snapshot->viewFamily.GetPrimaryView() != nullptr, "view family exposes primary view");

        Check(world.DestroyObject(object), "render world destroys proxy");
        Check(snapshot->objects.size() == 1, "existing snapshot remains immutable after world mutation");
        Check(world.GetObject(object) == nullptr, "stale render object handle is rejected");
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
        const PipelineHandle pipeline = device.CreateGraphicsPipeline({});
        device.SetDebugName(buffer, "Baseline.Buffer");
        device.SetDebugName(texture, "Baseline.Texture");
        device.SetDebugName(pipeline, "Baseline.Pipeline");

        const Shader::ShaderProgramInterface interface{
            .resources = {
                { .name = "Buffer", .set = 0, .binding = 0, .type = Shader::ShaderDescriptorType::UniformBuffer, .arrayCount = 1 },
                { .name = "Texture", .set = 0, .binding = 1, .type = Shader::ShaderDescriptorType::CombinedImageSampler, .arrayCount = 1 },
            },
        };
        const ResourceBindingHandle bufferBinding = ResolveResourceBinding(interface, "Buffer");
        const ResourceBindingHandle textureBinding = ResolveResourceBinding(interface, "Texture");
        ResourceBindingGroup bindings{ .set = 0 };
        bindings.BindBuffer(bufferBinding, buffer, 0, 64);
        bindings.BindTexture(textureBinding, texture);

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
        Check(statistics.descriptorUpdateCount == 2, "RHI statistics count descriptor writes");
        Check(device.resourceNames == std::vector<std::string>({ "Baseline.Buffer", "Baseline.Texture", "Baseline.Pipeline" }), "RHI resources accept debug names");
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
    }
} // namespace

int main()
{
    TestFrameStatisticsReset();
    TestDrawCommandDefaults();
    TestRenderWorldLifecycleAndSnapshot();
    TestRHIStatisticsAndNames();
    TestRenderGraphCompileAndExecuteOrder();

    if (g_failures != 0)
        std::cerr << g_failures << " render baseline test(s) failed\n";
    return g_failures == 0 ? 0 : 1;
}
