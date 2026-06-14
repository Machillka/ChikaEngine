#include "ChikaEngine/RenderPipeline.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RenderDebugVisualization.hpp"
#include "ChikaEngine/RenderPassBuilder.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace ChikaEngine::Render
{
    RenderPipeline::~RenderPipeline()
    {
        Shutdown();
    }

    void RenderPipeline::Initialize(const RenderPipelineCreateInfo& createInfo)
    {
        Shutdown();
        m_width = createInfo.width;
        m_height = createInfo.height;
        m_viewportWidth = createInfo.width;
        m_viewportHeight = createInfo.height;
        m_assetMgr = createInfo.assetManager;
        m_resourceMgr = createInfo.resourceManager;
        m_rhi = createInfo.rhi;
        m_settings = createInfo.settings;

        if (!m_assetMgr || !m_resourceMgr || !m_rhi || !m_settings)
            throw std::invalid_argument("RenderPipelineCreateInfo dependencies must not be null");
        m_renderGraph = std::make_unique<RenderGraph>(m_rhi);

        // 场景 UBO 创建
        BufferDesc uboDesc{
            .size = sizeof(SceneData),
            .usage = Render::RHI_BufferUsage::Uniform,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };
        m_sceneUBO = m_rhi->CreateBuffer(uboDesc);
        m_rhi->SetDebugName(m_sceneUBO, "Renderer.SceneData");

        // 初始化 Scene UBO 为默认值
        {
            SceneData* sceneData = static_cast<SceneData*>(m_rhi->GetMappedData(m_sceneUBO));
            if (sceneData)
            {
                sceneData->cameraVP = Math::Mat4::Identity().Transposed();
                sceneData->lightVP = Math::Mat4::Identity().Transposed();

                Math::Vector3 defaultLightDir(0.5f, -1.0f, 0.3f);
                Math::Vector3 n = defaultLightDir.Normalized();
                sceneData->lightDir[0] = n.x;
                sceneData->lightDir[1] = n.y;
                sceneData->lightDir[2] = n.z;
                sceneData->lightDir[3] = 0.0f;

                // 默认摄像机位置在原点
                sceneData->viewPos[0] = 0.0f;
                sceneData->viewPos[1] = 0.0f;
                sceneData->viewPos[2] = 0.0f;
                sceneData->viewPos[3] = 1.0f;
            }
        }

        // bone 的 ubo
        BufferDesc dummyBoneDesc{
            .size = sizeof(Math::Mat4),
            .usage = Render::RHI_BufferUsage::Storage,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };
        m_dummyBoneUBO = m_rhi->CreateBuffer(dummyBoneDesc);
        m_rhi->SetDebugName(m_dummyBoneUBO, "Renderer.DummyBoneData");

        // 填充空数据
        Math::Mat4* mappedBones = static_cast<Math::Mat4*>(m_rhi->GetMappedData(m_dummyBoneUBO));
        if (mappedBones)
            *mappedBones = Math::Mat4::Identity().Transposed();

        // 离屏纹理
        TextureDesc colorDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::BGRA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_offscreenColor = m_rhi->CreateTexture(colorDesc);
        m_rhi->SetDebugName(m_offscreenColor, "Renderer.OffscreenColor");

        // 深度 dump texture
        TextureDesc dummyDesc{
            .width = 1,
            .height = 1,
            .format = Render::RHI_Format::RGBA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::Sampled,
        };
        m_dummyTexture = m_rhi->CreateTexture(dummyDesc);
        m_rhi->SetDebugName(m_dummyTexture, "Renderer.DummyTexture");

        // 深度纹理
        // FIXED: 修改成 view port 大小
        TextureDesc depthDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment,
        };
        m_depthTexture = m_rhi->CreateTexture(depthDesc);
        m_rhi->SetDebugName(m_depthTexture, "Renderer.SceneDepth");
        m_rgDepth = m_renderGraph->ImportTexture("Depth", m_depthTexture, depthDesc);

        // Shadow
        Render::TextureDesc shadowDepthDesc{
            .width = m_settings->shadowResolution,
            .height = m_settings->shadowResolution,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_shadowDepthTexture = m_rhi->CreateTexture(shadowDepthDesc);
        m_rhi->SetDebugName(m_shadowDepthTexture, "Renderer.ShadowDepth");
        m_rgShadowDepth = m_renderGraph->ImportTexture("ShadowDepth", m_shadowDepthTexture, shadowDepthDesc);

        Render::TextureDesc shadowColorDesc{
            .width = m_settings->shadowResolution,
            .height = m_settings->shadowResolution,
            .format = Render::RHI_Format::BGRA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment,
        };
        m_shadowColorTexture = m_rhi->CreateTexture(shadowColorDesc);
        m_rhi->SetDebugName(m_shadowColorTexture, "Renderer.ShadowColorDummy");
        m_rgShadowColor = m_renderGraph->ImportTexture("ShadowColorDummy", m_shadowColorTexture, shadowColorDesc);

        CreateDeferredResources();

        Render::TextureDesc swapDesc{
            .width = m_width,
            .height = m_height,
            .format = Render::RHI_Format::BGRA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment,
        };
        m_rgSwapchain = m_renderGraph->ImportTexture("Swapchain", TextureHandle{}, swapDesc);
    }

    void RenderPipeline::SubmitSnapshot(std::shared_ptr<const RenderWorldSnapshot> snapshot)
    {
        m_snapshot = std::move(snapshot);
    }

    void RenderPipeline::SubmitImGuiData(void* drawData)
    {
        m_imguiDrawData = drawData;
    }

    void RenderPipeline::BuildRenderGraph()
    {
        m_renderGraph->Clear();

        Render::TextureDesc offscreenDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::BGRA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_rgOffscreen = m_renderGraph->ImportTexture("OffscreenColor", m_offscreenColor, offscreenDesc);

        Render::TextureDesc depthDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment,
        };
        m_rgDepth = m_renderGraph->ImportTexture("Depth", m_depthTexture, depthDesc);

        Render::TextureDesc shadowDepthDesc{
            .width = m_settings->shadowResolution,
            .height = m_settings->shadowResolution,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_rgShadowDepth = m_renderGraph->ImportTexture("ShadowDepth", m_shadowDepthTexture, shadowDepthDesc);

        Render::TextureDesc shadowColorDesc{
            .width = m_settings->shadowResolution,
            .height = m_settings->shadowResolution,
            .format = Render::RHI_Format::BGRA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment,
        };
        m_rgShadowColor = m_renderGraph->ImportTexture("ShadowColorDummy", m_shadowColorTexture, shadowColorDesc);

        Render::TextureDesc swapDesc{
            .width = m_width,
            .height = m_height,
            .format = Render::RHI_Format::BGRA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment,
        };

        // 每一帧获取最新的 Backbuffer 导入
        m_rgSwapchain = m_renderGraph->ImportTexture("Swapchain", m_rhi->GetActiveSwapchainTexture(), swapDesc);

        AddUploadPasses();
        AddShadowPass();
        if (m_settings->pipelineMode == RenderPipelineMode::Deferred)
        {
            AddGBufferPass();
            AddDeferredLightingPass();
            AddTransparentPass();
        }
        else
        {
            AddMainScenePass();
        }
        AddImGuiPass();

        m_renderGraph->AddPresentPass("Present", m_rgSwapchain);

        m_renderGraph->Compile();
    }

    void RenderPipeline::AddUploadPasses()
    {
        auto bufferJobs = m_resourceMgr->GetBufferUploadJobs();
        auto textureJobs = m_resourceMgr->GetTextureUploadJobs();

        if (bufferJobs.empty() && textureJobs.empty() && m_dummyTextureTransitioned)
            return;

        m_renderGraph->AddUploadPass("Upload Resources",
                                     [this, bufferJobs, textureJobs](IRHICommandList* cmd, RenderGraph* graph)
                                     {
                                         if (!m_dummyTextureTransitioned)
                                         {
                                             cmd->InsertTextureBarrier(m_dummyTexture, ResourceState::Undefined, ResourceState::ShaderResource);
                                             m_dummyTextureTransitioned = true;
                                         }
                                         // Buffer 上传：只需要 CopyBuffer
                                         for (const auto& job : bufferJobs)
                                         {
                                             cmd->CopyBuffer(job.staging, job.dst, job.size);
                                         }

                                         // Texture 上传：需要 barrier + CopyBufferToTexture + barrier
                                         for (const auto& job : textureJobs)
                                         {
                                             cmd->InsertTextureBarrier(job.dst, ResourceState::Undefined, ResourceState::TransferDst);
                                             cmd->CopyBufferToTexture(job.staging, job.dst, job.width, job.height);
                                             cmd->InsertTextureBarrier(job.dst, ResourceState::TransferDst, ResourceState::ShaderResource);
                                         }
                                     });
    }

    void RenderPipeline::AddShadowPass()
    {
        m_renderGraph->AddPass(
            "Shadow Pass",
            [&](RGPassBuilder& builder)
            {
                const float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                builder.WriteColor(m_rgShadowColor, LoadOp::Clear, clearColor);
                builder.WriteDepth(m_rgShadowDepth, LoadOp::Clear);
            },
            [this](IRHICommandList* cmd, RenderGraph* graph) { DrawRenderQueue(cmd, m_renderQueues.shadow); });
    }

    void RenderPipeline::AddMainScenePass()
    {
        m_renderGraph->AddPass(
            "Main Scene Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(m_rgShadowDepth, ResourceState::ShaderResource);
                const float clearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
                // builder.WriteColor(m_rgSwapchain, LoadOp::Clear, clearColor);
                // builder.WriteDepth(m_rgDepth, LoadOp::Clear);
                builder.WriteColor(m_rgOffscreen, LoadOp::Clear, clearColor);
                builder.WriteDepth(m_rgDepth, LoadOp::Clear);
            },
            [this](IRHICommandList* cmd, RenderGraph* graph)
            {
                DrawRenderQueue(cmd, m_renderQueues.forwardOpaque);
                DrawRenderQueue(cmd, m_renderQueues.forwardTransparent);
            });
    }
    void RenderPipeline::AddImGuiPass()
    {
        m_renderGraph->AddPass(
            "ImGui UI Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(m_rgOffscreen, ResourceState::ShaderResource);

                const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                builder.WriteColor(m_rgSwapchain, LoadOp::Clear, clearColor);
            },
            [this](IRHICommandList* cmd, RenderGraph* graph)
            {
                if (m_imguiDrawData)
                {
                    cmd->DrawImGui(m_imguiDrawData);
                }
            });
    }

    void RenderPipeline::AddGBufferPass()
    {
        Render::TextureDesc gbufferAlbedoDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::RGBA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment | Render::RHI_TextureUsage::Sampled,
        };
        Render::TextureDesc gbufferNormalDesc = gbufferAlbedoDesc;
        gbufferNormalDesc.format = Render::RHI_Format::RGBA16_Float;
        Render::TextureDesc gbufferMaterialDesc = gbufferAlbedoDesc;

        m_rgGBufferAlbedo = m_renderGraph->_RegisterTexture("GBuffer.Albedo", gbufferAlbedoDesc);
        m_rgGBufferNormal = m_renderGraph->_RegisterTexture("GBuffer.Normal", gbufferNormalDesc);
        m_rgGBufferMaterial = m_renderGraph->_RegisterTexture("GBuffer.Material", gbufferMaterialDesc);

        m_renderGraph->AddPass(
            "Deferred GBuffer Pass",
            [&](RGPassBuilder& builder)
            {
                const float clearColor[4] = { 0.02f, 0.02f, 0.02f, 1.0f };
                builder.WriteColor(m_rgGBufferAlbedo, LoadOp::Clear, clearColor);
                builder.WriteColor(m_rgGBufferNormal, LoadOp::Clear, clearColor);
                builder.WriteColor(m_rgGBufferMaterial, LoadOp::Clear, clearColor);
                builder.WriteDepth(m_rgDepth, LoadOp::Clear);
            },
            [this](IRHICommandList* cmd, RenderGraph* graph) { DrawRenderQueue(cmd, m_renderQueues.gbufferOpaque); });
    }

    void RenderPipeline::AddDeferredLightingPass()
    {
        m_renderGraph->AddPass(
            "Deferred Lighting Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(m_rgGBufferAlbedo, ResourceState::ShaderResource);
                builder.ReadTexture(m_rgGBufferNormal, ResourceState::ShaderResource);
                builder.ReadTexture(m_rgGBufferMaterial, ResourceState::ShaderResource);
                builder.ReadTexture(m_rgShadowDepth, ResourceState::ShaderResource);

                const float clearColor[4] = { 0.03f, 0.03f, 0.03f, 1.0f };
                builder.WriteColor(m_rgOffscreen, LoadOp::Clear, clearColor);
            },
            [this](IRHICommandList* cmd, RenderGraph* graph)
            {
                if (!m_deferredLightingPipeline.IsValid())
                    return;

                std::vector<Render::ResourceBindingGroup> bindings;
                Render::BindBuffer(bindings, m_deferredSceneBinding, m_sceneUBO, 0, sizeof(SceneData));
                Render::BindTexture(bindings, m_deferredAlbedoBinding, graph->GetPhysicalTexture(m_rgGBufferAlbedo));
                Render::BindTexture(bindings, m_deferredNormalBinding, graph->GetPhysicalTexture(m_rgGBufferNormal));
                Render::BindTexture(bindings, m_deferredMaterialBinding, graph->GetPhysicalTexture(m_rgGBufferMaterial));

                cmd->BindPipeline(m_deferredLightingPipeline);
                for (const auto& group : bindings)
                    cmd->BindResources(group);
                cmd->Draw(3, 1);
            });
    }

    /**
     * @brief 在 Deferred Lighting 后按远到近提交透明 Queue。
     *
     * 透明对象不写入 GBuffer；独立 Pass 保留已有颜色和深度，为 Phase 6 的正式透明材质路径提供边界。
     */
    void RenderPipeline::AddTransparentPass()
    {
        if (m_renderQueues.forwardTransparent.packets.empty())
            return;

        m_renderGraph->AddPass(
            "Forward Transparent Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(m_rgShadowDepth, ResourceState::ShaderResource);
                builder.WriteColor(m_rgOffscreen, LoadOp::Load);
                builder.WriteDepth(m_rgDepth, LoadOp::Load);
            },
            [this](IRHICommandList* cmd, RenderGraph* graph) { DrawRenderQueue(cmd, m_renderQueues.forwardTransparent); });
    }

    void RenderPipeline::DrawRenderQueue(IRHICommandList* cmd, const RenderQueue& queue)
    {
        PipelineHandle boundPipeline;
        Resource::MeshHandle boundMesh;
        Resource::MaterialHandle boundMaterial;
        BufferHandle boundBoneBuffer;

        for (const RenderBatch& batch : queue.batches)
        {
            const Resource::MeshGPU& mesh = m_resourceMgr->GetMesh(batch.mesh);
            const Resource::MaterialGPU& material = m_resourceMgr->GetMaterial(batch.material);
            if (!batch.pipeline.IsValid() || !mesh.vertexBuffer.IsValid() || !mesh.indexBuffer.IsValid() || batch.packetCount == 0)
                continue;

            // 排序后的相邻 Batch 通常共享 Pipeline 或 Mesh，录制时避免重复绑定相同状态。
            if (batch.mesh != boundMesh)
            {
                cmd->BindVertexBuffer(mesh.vertexBuffer, 0);
                cmd->BindIndexBuffer(mesh.indexBuffer, 0, mesh.isUint32);
                boundMesh = batch.mesh;
            }
            if (batch.pipeline != boundPipeline)
            {
                cmd->BindPipeline(batch.pipeline);
                boundPipeline = batch.pipeline;
            }

            const bool shadowPass = batch.pass == RenderPassClass::Shadow;
            const bool gbufferPass = batch.pass == RenderPassClass::GBufferOpaque;
            const Resource::MaterialDrawBindings& drawBindings = gbufferPass ? material.gbufferDrawBindings : material.forwardDrawBindings;
            PC pc{};
            pc.isShadowPass = shadowPass ? 1 : 0;
            pc.renderMode = gbufferPass ? 1 : 0;
            BufferHandle boneBuffer = m_dummyBoneUBO;
            uint64_t boneBufferSize = sizeof(Math::Mat4);

            if (batch.instanced)
            {
                pc.model = Math::Mat4::Identity();
                pc.useInstancing = 1;
            }
            else
            {
                const RenderPacket& packet = queue.packets[batch.firstPacket];
                const RenderObjectProxy& proxy = packet.object->proxy;
                const uint64_t objectKey = (m_snapshot->worldId << 32u) ^ packet.object->handle.raw_value;
                const auto objectBoneBuffer = m_boneBuffers.find(objectKey);
                if (HasFlag(proxy.flags, RenderObjectFlags::Skinned) && objectBoneBuffer != m_boneBuffers.end())
                {
                    boneBuffer = objectBoneBuffer->second.handle;
                    boneBufferSize = objectBoneBuffer->second.size;
                    pc.isSkinned = 1;
                }
                pc.model = proxy.transform.Transposed();
            }

            // 相同 Material 与对象数据绑定可跨 Batch 复用；蒙皮对象因骨骼 Buffer 不同会自然失效。
            if (batch.material != boundMaterial || boneBuffer != boundBoneBuffer)
            {
                const DynamicBuffer& instances = m_instanceBuffers[m_instanceBufferIndex];
                auto bindings = material.bindings;
                Render::BindBuffer(bindings, drawBindings.scene, m_sceneUBO, 0, sizeof(SceneData));
                Render::BindTexture(bindings, drawBindings.shadowMap, shadowPass ? m_dummyTexture : m_shadowDepthTexture);
                Render::BindBuffer(bindings, drawBindings.instances, instances.handle, 0, instances.size);
                Render::BindBuffer(bindings, drawBindings.bones, boneBuffer, 0, boneBufferSize);
                for (const auto& group : bindings)
                    cmd->BindResources(group);
                boundMaterial = batch.material;
                boundBoneBuffer = boneBuffer;
            }

            cmd->PushConstants("pc", &pc, sizeof(PC));
            cmd->DrawIndexed(mesh.indexCount, batch.instanced ? static_cast<uint32_t>(batch.packetCount) : 1, 0, 0, batch.instanced ? batch.firstInstance : 0);
        }
    }

    void RenderPipeline::CreateDeferredResources()
    {
        Asset::ShaderHandle vsAsset = m_assetMgr->LoadShader("Assets/Shaders/fullscreen.vert");
        Asset::ShaderHandle fsAsset = m_assetMgr->LoadShader("Assets/Shaders/deferred_lighting.frag");
        const auto* vsSpirv = m_assetMgr->GetShader(vsAsset);
        const auto* fsSpirv = m_assetMgr->GetShader(fsAsset);
        if (!vsSpirv || !fsSpirv)
            return;
        if (!vsSpirv->hasReflection || !fsSpirv->hasReflection)
            return;
        const std::array reflectedStages{ vsSpirv->reflection, fsSpirv->reflection };
        Shader::ShaderProgramBuildResult interfaceResult = Shader::BuildShaderProgramInterface(reflectedStages);
        if (!interfaceResult.success)
        {
            for (const std::string& error : interfaceResult.errors)
                LOG_ERROR("Renderer", "Deferred shader interface conflict: {}", error);
            return;
        }
        m_deferredLightingInterface = std::move(interfaceResult.interface);
        m_deferredSceneBinding = ResolveResourceBinding(m_deferredLightingInterface, "scene");
        m_deferredAlbedoBinding = ResolveResourceBinding(m_deferredLightingInterface, "GBufferAlbedo");
        m_deferredNormalBinding = ResolveResourceBinding(m_deferredLightingInterface, "GBufferNormal");
        m_deferredMaterialBinding = ResolveResourceBinding(m_deferredLightingInterface, "GBufferMaterial");

        m_deferredLightingVertexShader = m_rhi->CreateShader({
            .stage = RHI_ShaderStage::Vertex,
            .code = vsSpirv->spirv.data(),
            .codeSize = vsSpirv->spirv.size(),
        });
        m_deferredLightingFragmentShader = m_rhi->CreateShader({
            .stage = RHI_ShaderStage::Fragment,
            .code = fsSpirv->spirv.data(),
            .codeSize = fsSpirv->spirv.size(),
        });

        PipelineDesc desc{
            .vertexShader = m_deferredLightingVertexShader,
            .fragmentShader = m_deferredLightingFragmentShader,
            .shaderInterface = m_deferredLightingInterface,
            .vertexLayout = {},
            .depthTest = false,
            .depthWrite = false,
            .alphaBlendEnable = false,
        };
        desc.colorAttachmentFormats.push_back(RHI_Format::BGRA8_UNorm);
        desc.depthAttachmentFormat = RHI_Format::Unknown;
        m_deferredLightingPipeline = m_rhi->CreateGraphicsPipeline(desc);
        m_rhi->SetDebugName(m_deferredLightingVertexShader, "Renderer.DeferredLighting.VertexShader");
        m_rhi->SetDebugName(m_deferredLightingFragmentShader, "Renderer.DeferredLighting.FragmentShader");
        m_rhi->SetDebugName(m_deferredLightingPipeline, "Renderer.DeferredLighting.Pipeline");
    }

    void RenderPipeline::BeginFrame()
    {
        // 清空 Renderer 侧汇总，避免跳帧时继续显示上一帧统计。
        m_frameStatistics.Reset();
        HandlePendingResize();
    }

    void RenderPipeline::Execute(float deltaTime)
    {
        m_time += deltaTime;
        UpdateSceneDataFromSnapshot();
        PrepareSnapshotResources();
        PrepareRenderQueues();
        PrepareInstanceData();

        BuildRenderGraph();

        m_renderGraph->Execute();

        // RHI 负责命令统计，RenderGraph 负责 Pass 统计；Renderer 在帧执行结束后汇总两者。
        m_frameStatistics = m_rhi->GetFrameStatistics();
        m_frameStatistics.passCount = m_renderGraph->GetLastExecutedPassCount();
        m_frameStatistics.visibleObjectCount = m_visibleObjectCount;
        m_frameStatistics.culledObjectCount = m_culledObjectCount;
        const std::array<const RenderQueue*, 3> queues{
            &m_renderQueues.shadow,
            m_settings->pipelineMode == RenderPipelineMode::Deferred ? &m_renderQueues.gbufferOpaque : &m_renderQueues.forwardOpaque,
            &m_renderQueues.forwardTransparent,
        };
        for (const RenderQueue* queue : queues)
        {
            m_frameStatistics.packetCount += static_cast<uint32_t>(queue->packets.size());
            m_frameStatistics.batchCount += static_cast<uint32_t>(queue->batches.size());
            m_frameStatistics.instancedBatchCount += static_cast<uint32_t>(std::ranges::count(queue->batches, true, &RenderBatch::instanced));
        }
    }

    void RenderPipeline::UpdateSceneDataFromSnapshot()
    {
        SceneData* sceneData = static_cast<SceneData*>(m_rhi->GetMappedData(m_sceneUBO));
        if (!sceneData || !m_snapshot)
            return;

        if (const RenderView* view = m_snapshot->viewFamily.GetPrimaryView())
        {
            sceneData->cameraVP = view->viewProjection.Transposed();
            sceneData->viewPos[0] = view->position.x;
            sceneData->viewPos[1] = view->position.y;
            sceneData->viewPos[2] = view->position.z;
            sceneData->viewPos[3] = 1.0f;
        }

        if (!m_snapshot->lights.empty())
        {
            const RenderLightProxy& light = m_snapshot->lights.front().proxy;
            sceneData->lightVP = light.viewProjection.Transposed();
            sceneData->lightDir[0] = light.direction.x;
            sceneData->lightDir[1] = light.direction.y;
            sceneData->lightDir[2] = light.direction.z;
            sceneData->lightDir[3] = 0.0f;
        }
    }

    void RenderPipeline::PrepareSnapshotResources()
    {
        std::unordered_set<uint64_t> aliveBoneObjects;
        if (m_snapshot)
        {
            for (const auto& object : m_snapshot->objects)
            {
                const uint64_t objectKey = (m_snapshot->worldId << 32u) ^ object.handle.raw_value;
                if (!HasFlag(object.proxy.flags, RenderObjectFlags::Skinned) || object.proxy.boneMatrices.empty())
                    continue;
                aliveBoneObjects.insert(objectKey);
                DynamicBuffer& allocation = m_boneBuffers[objectKey];
                const uint64_t requiredSize = object.proxy.boneMatrices.size() * sizeof(Math::Mat4);
                if (!allocation.handle.IsValid() || allocation.capacity < requiredSize)
                {
                    if (allocation.handle.IsValid())
                        m_rhi->DestroyBuffer(allocation.handle);
                    allocation.handle = m_rhi->CreateBuffer({
                        .size = requiredSize,
                        .usage = RHI_BufferUsage::Storage,
                        .memoryUsage = MemoryUsage::CPU_To_GPU,
                    });
                    allocation.capacity = requiredSize;
                    m_rhi->SetDebugName(allocation.handle, "Animation.BoneMatrices");
                }

                allocation.size = requiredSize;
                auto* mapped = static_cast<Math::Mat4*>(m_rhi->GetMappedData(allocation.handle));
                if (mapped)
                {
                    for (size_t index = 0; index < object.proxy.boneMatrices.size(); ++index)
                        mapped[index] = object.proxy.boneMatrices[index].Transposed();
                }
            }
        }

        for (auto it = m_boneBuffers.begin(); it != m_boneBuffers.end();)
        {
            if (aliveBoneObjects.contains(it->first))
            {
                ++it;
                continue;
            }
            m_rhi->DestroyBuffer(it->second.handle);
            it = m_boneBuffers.erase(it);
        }
    }

    /**
     * @brief 从当前 Snapshot 构建主视图与阴影视图可见集合，再分类为独立 Pass Queue。
     *
     * Pipeline 后续阶段只处理可见 Packet，不再读取全部 Render Proxy 或使用 Pass 布尔分支。
     */
    void RenderPipeline::PrepareRenderQueues()
    {
        m_renderQueues = {};
        m_visibleObjectCount = 0;
        m_culledObjectCount = 0;
        if (!m_snapshot)
            return;

        const RenderView* primaryView = m_snapshot->viewFamily.GetPrimaryView();
        if (!primaryView)
            return;

        const VisibilityResult mainVisibility = BuildVisibility(*m_snapshot, *primaryView);
        VisibilityResult shadowVisibility;
        if (!m_snapshot->lights.empty())
        {
            const RenderLightProxy& light = m_snapshot->lights.front().proxy;
            RenderView shadowView{
                .viewProjection = light.viewProjection,
                .layerMask = light.layerMask,
            };
            shadowVisibility = BuildVisibility(*m_snapshot, shadowView, true);
        }
        else
            shadowVisibility = BuildVisibility(*m_snapshot, *primaryView, true);

        m_visibleObjectCount = mainVisibility.visibleObjectCount;
        m_culledObjectCount = mainVisibility.culledObjectCount;
        m_renderQueues = BuildRenderQueues(mainVisibility, shadowVisibility, *primaryView, *m_resourceMgr);
    }

    /**
     * @brief 将所有静态 Batch 的 World Matrix 写入三缓冲 Instance Storage Buffer。
     *
     * 三缓冲避免 CPU 覆盖仍被前序 GPU 帧读取的数据；只有容量增长时才等待设备并重建 Buffer。
     */
    void RenderPipeline::PrepareInstanceData()
    {
        m_instanceBufferIndex = (m_instanceBufferIndex + 1u) % static_cast<uint32_t>(m_instanceBuffers.size());
        // 第 0 项始终提供合法 Dummy Descriptor，非实例对象不会读取该矩阵。
        std::vector<Math::Mat4> instanceMatrices{ Math::Mat4::Identity().Transposed() };
        const std::array<RenderQueue*, 3> queues{
            &m_renderQueues.shadow,
            m_settings->pipelineMode == RenderPipelineMode::Deferred ? &m_renderQueues.gbufferOpaque : &m_renderQueues.forwardOpaque,
            &m_renderQueues.forwardTransparent,
        };
        for (RenderQueue* queue : queues)
        {
            for (RenderBatch& batch : queue->batches)
            {
                if (!batch.instanced)
                    continue;
                batch.firstInstance = static_cast<uint32_t>(instanceMatrices.size());
                for (size_t packetOffset = 0; packetOffset < batch.packetCount; ++packetOffset)
                    instanceMatrices.push_back(queue->packets[batch.firstPacket + packetOffset].object->proxy.transform.Transposed());
            }
        }

        DynamicBuffer& allocation = m_instanceBuffers[m_instanceBufferIndex];
        const uint64_t requiredSize = instanceMatrices.size() * sizeof(Math::Mat4);
        allocation.size = requiredSize;
        if (requiredSize == 0)
            return;

        if (!allocation.handle.IsValid() || allocation.capacity < requiredSize)
        {
            m_rhi->WaitIdle();
            if (allocation.handle.IsValid())
                m_rhi->DestroyBuffer(allocation.handle);
            allocation.handle = m_rhi->CreateBuffer({
                .size = requiredSize,
                .usage = RHI_BufferUsage::Storage,
                .memoryUsage = MemoryUsage::CPU_To_GPU,
            });
            allocation.capacity = requiredSize;
            m_rhi->SetDebugName(allocation.handle, "Renderer.InstanceMatrices");
        }

        if (void* mapped = m_rhi->GetMappedData(allocation.handle))
            std::memcpy(mapped, instanceMatrices.data(), requiredSize);
    }

    void RenderPipeline::Shutdown()
    {
        if (m_rhi)
            m_rhi->WaitIdle();

        if (m_renderGraph)
            m_renderGraph->Clear();

        m_imguiDrawData = nullptr;
        m_dummyTextureTransitioned = false;
        if (m_rhi)
        {
            for (const auto& [object, buffer] : m_boneBuffers)
                m_rhi->DestroyBuffer(buffer.handle);
            DestroyOwnedResources();
        }
        m_boneBuffers.clear();
        m_renderQueues = {};
        m_snapshot.reset();
        m_renderGraph.reset();
        m_resourceMgr = nullptr;
        m_rhi = nullptr;
        m_assetMgr = nullptr;
        m_settings = nullptr;
    }

    /**
     * @brief 显式释放 RenderPipeline 创建的 RHI 资源。
     *
     * 资源所有权跟随创建者，避免 Facade 拆分后依赖设备 Shutdown 才兜底回收。
     * Vulkan 后端会把 Destroy 请求放入延迟删除队列，因此这里先由 Shutdown 等待 GPU 空闲。
     */
    void RenderPipeline::DestroyOwnedResources()
    {
        auto destroyBuffer = [this](BufferHandle& handle)
        {
            if (handle.IsValid())
                m_rhi->DestroyBuffer(handle);
            handle = {};
        };
        auto destroyTexture = [this](TextureHandle& handle)
        {
            if (handle.IsValid())
                m_rhi->DestroyTexture(handle);
            handle = {};
        };
        auto destroyShader = [this](ShaderHandle& handle)
        {
            if (handle.IsValid())
                m_rhi->DestroyShader(handle);
            handle = {};
        };
        auto destroyPipeline = [this](PipelineHandle& handle)
        {
            if (handle.IsValid())
                m_rhi->DestroyPipeline(handle);
            handle = {};
        };

        destroyPipeline(m_deferredLightingPipeline);
        destroyShader(m_deferredLightingVertexShader);
        destroyShader(m_deferredLightingFragmentShader);
        destroyBuffer(m_sceneUBO);
        destroyBuffer(m_dummyBoneUBO);
        destroyTexture(m_offscreenColor);
        destroyTexture(m_depthTexture);
        destroyTexture(m_dummyTexture);
        destroyTexture(m_shadowDepthTexture);
        destroyTexture(m_shadowColorTexture);
        for (DynamicBuffer& allocation : m_instanceBuffers)
        {
            destroyBuffer(allocation.handle);
            allocation = {};
        }
    }

    /*!
     * @brief  提供 resize 的标记, 这样在一次 drawcall 之间多次调用只会记录最后一次数据
     *
     * @param  width
     * @param  height
     * @author Machillka (machillka2007@gmail.com)
     * @date 2026-04-26
     */
    // void RenderPipeline::RequestResize(uint32_t width, uint32_t height)
    // {
    //     if (width == 0 || height == 0)
    //         return;
    //     if (width != m_viewportWidth || height != m_viewportHeight)
    //     {
    //         m_isResizePending = true;
    //         m_pendingWidth = width;
    //         m_pendingHeight = height;
    //     }
    // }

    void RenderPipeline::HandlePendingResize()
    {
        if (!m_isViewResizePending)
            return;

        // NOTE: 在销毁任何纹理或重建 Swapchain 前，必须确保 GPU 已经跑完所有正在执行的渲染指令！
        if (m_rhi)
        {
            m_rhi->WaitIdle();
        }

        m_viewportWidth = m_pendingViewWidth;
        m_viewportHeight = m_pendingViewHeight;
        m_isViewResizePending = false;

        m_rhi->DestroyTexture(m_offscreenColor);
        m_rhi->DestroyTexture(m_depthTexture);

        // 重建 Color
        TextureDesc colorDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::BGRA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_offscreenColor = m_rhi->CreateTexture(colorDesc);
        m_rhi->SetDebugName(m_offscreenColor, "Renderer.OffscreenColor");

        // 重建 Depth
        TextureDesc depthDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment,
        };
        m_depthTexture = m_rhi->CreateTexture(depthDesc);
        m_rhi->SetDebugName(m_depthTexture, "Renderer.SceneDepth");

        // 通知 RenderGraph 更新物理句柄
        m_renderGraph->UpdateImportedTexture(m_rgOffscreen, m_offscreenColor);
        m_renderGraph->UpdateImportedTexture(m_rgDepth, m_depthTexture);

        LOG_INFO("Renderer", "Successfully resized to {}x{}", m_viewportWidth, m_viewportHeight);
    }

    void RenderPipeline::OnViewResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        if (width != m_viewportWidth || height != m_viewportHeight)
        {
            m_isViewResizePending = true;
            m_pendingViewWidth = width;
            m_pendingViewHeight = height;
        }
    }

    void RenderPipeline::OnWindowResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;
        m_width = width;
        m_height = height;
    }

    uint32_t RenderPipeline::GetViewportWidth() const
    {
        return m_viewportWidth;
    }

    uint32_t RenderPipeline::GetViewportHeight() const
    {
        return m_viewportHeight;
    }

    TextureHandle RenderPipeline::GetOffscreenTexture() const
    {
        return m_offscreenColor;
    }

    const RenderFrameStatistics& RenderPipeline::GetFrameStatistics() const
    {
        return m_frameStatistics;
    }

    void RenderPipeline::AppendDebugGizmos() const
    {
        if (!m_snapshot || !m_settings)
            return;
        AppendRenderWorldDebugGizmos(*m_snapshot,
                                     {
                                         .drawAABBs = m_settings->debugDrawAABBs,
                                         .drawFrustums = m_settings->debugDrawFrustums,
                                     });
    }

} // namespace ChikaEngine::Render
