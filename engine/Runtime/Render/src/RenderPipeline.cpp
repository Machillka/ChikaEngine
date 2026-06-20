#include "ChikaEngine/RenderPipeline.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RenderDebugVisualization.hpp"
#include "ChikaEngine/RenderPassBuilder.hpp"
#include "ChikaEngine/RenderPipelinePasses.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/jobs/JobSystem.hpp"
#include "ChikaEngine/profiler/ProfilerClock.hpp"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace ChikaEngine::Render
{
    namespace
    {
        constexpr uint32_t MAX_RENDER_LIGHTS = 128;
    }

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
        m_jobSystem = createInfo.jobSystem;
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

                // 默认摄像机位置在原点
                sceneData->viewPos[0] = 0.0f;
                sceneData->viewPos[1] = 0.0f;
                sceneData->viewPos[2] = 0.0f;
                sceneData->viewPos[3] = 1.0f;
                sceneData->frameOptions[0] = m_settings->ambientIntensity;
                sceneData->frameOptions[1] = 0.0f;
                sceneData->frameOptions[2] = m_settings->shadows.depthBias;
                sceneData->frameOptions[3] = m_settings->shadows.normalBias;
                sceneData->shadowOptions[0] = 1.0f / static_cast<float>(m_settings->shadows.resolution);
                sceneData->shadowOptions[1] = sceneData->shadowOptions[0];
                sceneData->shadowOptions[2] = static_cast<float>(m_settings->shadows.pcfRadius);
                sceneData->shadowOptions[3] = 0.0f;
            }
        }

        // Light Buffer 使用固定上限建立稳定 Descriptor；当前 Shader 正确遍历全部有效光源。
        m_lightBuffer = m_rhi->CreateBuffer({
            .size = sizeof(LightGPU) * MAX_RENDER_LIGHTS,
            .usage = Render::RHI_BufferUsage::Storage,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        });
        m_rhi->SetDebugName(m_lightBuffer, "Renderer.LightData");
        if (void* mapped = m_rhi->GetMappedData(m_lightBuffer))
            std::memset(mapped, 0, sizeof(LightGPU) * MAX_RENDER_LIGHTS);

        m_postProcessUBO = m_rhi->CreateBuffer({
            .size = sizeof(PostProcessData),
            .usage = Render::RHI_BufferUsage::Uniform,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        });
        m_rhi->SetDebugName(m_postProcessUBO, "Renderer.PostProcessData");
        UpdatePostProcessData();

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
            .format = Render::RHI_Format::RGBA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_offscreenColor = m_rhi->CreateTexture(colorDesc);
        m_rhi->SetDebugName(m_offscreenColor, "Renderer.OffscreenColor");

        // 场景始终在线性 HDR 目标中渲染，最终由 Post Process 输出到 LDR Viewport Texture。
        colorDesc.format = Render::RHI_Format::RGBA16_Float;
        m_hdrSceneColor = m_rhi->CreateTexture(colorDesc);
        m_rhi->SetDebugName(m_hdrSceneColor, "Renderer.HDRSceneColor");

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

        // Shadow
        Render::TextureDesc shadowDepthDesc{
            .width = m_settings->shadows.resolution,
            .height = m_settings->shadows.resolution,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_shadowDepthTexture = m_rhi->CreateTexture(shadowDepthDesc);
        m_rhi->SetDebugName(m_shadowDepthTexture, "Renderer.ShadowDepth");

        CreateDeferredResources();
        CreatePostProcessResources();

        Render::TextureDesc swapDesc{
            .width = m_width,
            .height = m_height,
            .format = Render::RHI_Format::RGBA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment,
        };
    }

    void RenderPipeline::SubmitSnapshot(std::shared_ptr<const RenderWorldSnapshot> snapshot)
    {
        m_snapshot = std::move(snapshot);
    }

    void RenderPipeline::SetOverlayPassCallback(RenderOverlayCallback callback)
    {
        m_overlayCallback = std::move(callback);
    }

    void RenderPipeline::BuildRenderGraph()
    {
        m_renderGraph->Clear();
        m_graphBlackboard.Clear();

        Render::TextureDesc offscreenDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::RGBA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_graphBlackboard.SetTexture(std::string(RenderGraphSemantic::SceneColor), m_renderGraph->ImportTexture("OffscreenColor", m_offscreenColor, offscreenDesc, ResourceState::Undefined, ResourceState::ShaderResource));
        offscreenDesc.format = RHI_Format::RGBA16_Float;
        m_graphBlackboard.SetTexture(std::string(RenderGraphSemantic::HDRSceneColor), m_renderGraph->ImportTexture("HDRSceneColor", m_hdrSceneColor, offscreenDesc, ResourceState::Undefined, ResourceState::ShaderResource));

        Render::TextureDesc depthDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment,
        };
        m_graphBlackboard.SetTexture(std::string(RenderGraphSemantic::SceneDepth), m_renderGraph->ImportTexture("Depth", m_depthTexture, depthDesc, ResourceState::Undefined, ResourceState::DepthWrite));

        Render::TextureDesc shadowDepthDesc{
            .width = m_settings->shadows.resolution,
            .height = m_settings->shadows.resolution,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_graphBlackboard.SetTexture(std::string(RenderGraphSemantic::ShadowDepth), m_renderGraph->ImportTexture("ShadowDepth", m_shadowDepthTexture, shadowDepthDesc, ResourceState::Undefined, ResourceState::ShaderResource));

        Render::TextureDesc swapDesc{
            .width = m_width,
            .height = m_height,
            .format = Render::RHI_Format::BGRA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment,
        };

        // 每一帧获取最新的 Backbuffer 导入
        m_graphBlackboard.SetTexture(std::string(RenderGraphSemantic::Swapchain), m_renderGraph->ImportTexture("Swapchain", m_rhi->GetActiveSwapchainTexture(), swapDesc, ResourceState::Undefined, ResourceState::Present));

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
        AddPostProcessPass();
        AddOverlayPass();

        m_renderGraph->AddPresentPass("Present", m_graphBlackboard.GetTexture(RenderGraphSemantic::Swapchain));

        m_renderGraph->Compile();
    }

    void RenderPipeline::AddUploadPasses()
    {
        auto bufferJobs = m_resourceMgr->GetBufferUploadJobs();
        auto textureJobs = m_resourceMgr->GetTextureUploadJobs();

        if (bufferJobs.empty() && textureJobs.empty() && m_dummyTextureTransitioned)
            return;

        m_renderGraph->AddCopyPass(
            "Upload Resources",
            [&](RGPassBuilder& builder)
            {
                for (size_t index = 0; index < bufferJobs.size(); ++index)
                {
                    const auto& job = bufferJobs[index];
                    const BufferDesc stagingDesc{ .size = job.size, .usage = RHI_BufferUsage::TransferSrc, .memoryUsage = MemoryUsage::CPU_To_GPU };
                    const BufferDesc destinationDesc{ .size = job.size, .usage = RHI_BufferUsage::TransferDst, .memoryUsage = MemoryUsage::GPU_Only };
                    const RGBufferHandle staging = m_renderGraph->ImportBuffer("Upload.Buffer.Staging." + std::to_string(index), job.staging, stagingDesc, ResourceState::CopySrc, ResourceState::CopySrc);
                    const RGBufferHandle destination = m_renderGraph->ImportBuffer("Upload.Buffer.Destination." + std::to_string(index), job.dst, destinationDesc, ResourceState::Undefined, job.finalState);
                    builder.ReadBuffer(staging, ResourceState::CopySrc, { 0, job.size });
                    builder.WriteBuffer(destination, ResourceState::CopyDst, { 0, job.size });
                }
                for (size_t index = 0; index < textureJobs.size(); ++index)
                {
                    const auto& job = textureJobs[index];
                    const BufferDesc stagingDesc{
                        .size = static_cast<uint64_t>(job.width) * job.height * 4u,
                        .usage = RHI_BufferUsage::TransferSrc,
                        .memoryUsage = MemoryUsage::CPU_To_GPU,
                    };
                    const TextureDesc destinationDesc{
                        .width = job.width,
                        .height = job.height,
                        .format = job.format,
                        .usage = RHI_TextureUsage::Sampled,
                    };
                    const RGBufferHandle staging = m_renderGraph->ImportBuffer("Upload.Texture.Staging." + std::to_string(index), job.staging, stagingDesc, ResourceState::CopySrc, ResourceState::CopySrc);
                    const RGTextureHandle destination = m_renderGraph->ImportTexture("Upload.Texture.Destination." + std::to_string(index), job.dst, destinationDesc, ResourceState::Undefined, ResourceState::ShaderResource);
                    builder.ReadBuffer(staging, ResourceState::CopySrc, { 0, stagingDesc.size });
                    builder.WriteTexture(destination, ResourceState::CopyDst);
                }
            },
            [this, bufferJobs, textureJobs](IRHICommandList* cmd, RenderGraph*)
            {
                if (!m_dummyTextureTransitioned)
                {
                    cmd->InsertTextureBarrier(m_dummyTexture, ResourceState::Undefined, ResourceState::ShaderResource);
                    m_dummyTextureTransitioned = true;
                }
                for (size_t index = 0; index < bufferJobs.size(); ++index)
                    cmd->CopyBuffer(bufferJobs[index].staging, bufferJobs[index].dst, bufferJobs[index].size);
                for (size_t index = 0; index < textureJobs.size(); ++index)
                    cmd->CopyBufferToTexture(textureJobs[index].staging, textureJobs[index].dst, textureJobs[index].width, textureJobs[index].height);
            });
    }

    void RenderPipeline::AddShadowPass()
    {
        PassModules::AddShadow(*m_renderGraph, m_graphBlackboard, [this](IRHICommandList* cmd, RenderGraph*) { DrawRenderQueue(cmd, m_renderQueues.shadow); });
    }

    void RenderPipeline::AddMainScenePass()
    {
        PassModules::AddForward(*m_renderGraph,
                                m_graphBlackboard,
                                [this](IRHICommandList* cmd, RenderGraph*)
                                {
                                    DrawRenderQueue(cmd, m_renderQueues.forwardOpaque);
                                    DrawRenderQueue(cmd, m_renderQueues.forwardTransparent);
                                });
    }
    void RenderPipeline::AddOverlayPass()
    {
        PassModules::AddOverlay(*m_renderGraph,
                                m_graphBlackboard,
                                [this](IRHICommandList* cmd, RenderGraph*)
                                {
                                    if (m_overlayCallback)
                                        m_overlayCallback(cmd);
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
        gbufferMaterialDesc.format = Render::RHI_Format::RGBA16_Float;
        Render::TextureDesc gbufferPositionDesc = gbufferNormalDesc;

        PassModules::AddGBuffer(*m_renderGraph,
                                m_graphBlackboard,
                                {
                                    .albedo = gbufferAlbedoDesc,
                                    .normal = gbufferNormalDesc,
                                    .material = gbufferMaterialDesc,
                                    .position = gbufferPositionDesc,
                                },
                                [this](IRHICommandList* cmd, RenderGraph*) { DrawRenderQueue(cmd, m_renderQueues.gbufferOpaque); });
    }

    void RenderPipeline::AddDeferredLightingPass()
    {
        PassModules::AddDeferredLighting(*m_renderGraph,
                                         m_graphBlackboard,
                                         [this](IRHICommandList* cmd, RenderGraph* graph)
                                         {
                                             if (!m_deferredLightingPipeline.IsValid())
                                                 return;

                                             std::vector<Render::ResourceBindingGroup> bindings;
                                             Render::BindBuffer(bindings, m_deferredSceneBinding, m_sceneUBO, 0, sizeof(SceneData));
                                             Render::BindTexture(bindings, m_deferredAlbedoBinding, graph->GetPhysicalTexture(m_graphBlackboard.GetTexture(RenderGraphSemantic::GBufferAlbedo)));
                                             Render::BindTexture(bindings, m_deferredNormalBinding, graph->GetPhysicalTexture(m_graphBlackboard.GetTexture(RenderGraphSemantic::GBufferNormal)));
                                             Render::BindTexture(bindings, m_deferredMaterialBinding, graph->GetPhysicalTexture(m_graphBlackboard.GetTexture(RenderGraphSemantic::GBufferMaterial)));
                                             Render::BindTexture(bindings, m_deferredPositionBinding, graph->GetPhysicalTexture(m_graphBlackboard.GetTexture(RenderGraphSemantic::GBufferPosition)));
                                             Render::BindBuffer(bindings, m_deferredLightsBinding, m_lightBuffer, 0, sizeof(LightGPU) * MAX_RENDER_LIGHTS);
                                             Render::BindTexture(bindings, m_deferredShadowBinding, m_shadowDepthTexture);

                                             cmd->BindPipeline(m_deferredLightingPipeline);
                                             for (const auto& group : bindings)
                                                 cmd->BindResources(group);
                                             cmd->Draw(3, 1);
                                         });
    }

    /**
     * @brief 在 Deferred Lighting 后按远到近提交透明 Queue。
     *
     * 透明对象不写入 GBuffer；独立 Pass 保留已有 HDR 颜色和深度，并在 Tone Mapping 前完成混合。
     */
    void RenderPipeline::AddTransparentPass()
    {
        if (m_renderQueues.forwardTransparent.packets.empty())
            return;

        PassModules::AddTransparent(*m_renderGraph, m_graphBlackboard, [this](IRHICommandList* cmd, RenderGraph*) { DrawRenderQueue(cmd, m_renderQueues.forwardTransparent); });
    }

    /**
     * @brief 在所有 HDR 几何与透明 Pass 完成后执行统一显示输出。
     */
    void RenderPipeline::AddPostProcessPass()
    {
        PassModules::AddPostProcess(*m_renderGraph,
                                    m_graphBlackboard,
                                    [this](IRHICommandList* cmd, RenderGraph* graph)
                                    {
                                        if (!m_postProcessPipeline.IsValid())
                                            return;
                                        std::vector<ResourceBindingGroup> bindings;
                                        BindTexture(bindings, m_postProcessSceneColorBinding, graph->GetPhysicalTexture(m_graphBlackboard.GetTexture(RenderGraphSemantic::HDRSceneColor)));
                                        BindBuffer(bindings, m_postProcessDataBinding, m_postProcessUBO, 0, sizeof(PostProcessData));
                                        cmd->BindPipeline(m_postProcessPipeline);
                                        for (const auto& group : bindings)
                                            cmd->BindResources(group);
                                        cmd->Draw(3, 1);
                                    });
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
            const Resource::MaterialDrawBindings& drawBindings = shadowPass ? material.shadowDrawBindings : (gbufferPass ? material.gbufferDrawBindings : material.forwardDrawBindings);
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
                auto bindings = shadowPass ? material.shadowBindings : material.bindings;
                Render::BindBuffer(bindings, drawBindings.scene, m_sceneUBO, 0, sizeof(SceneData));
                Render::BindTexture(bindings, drawBindings.shadowMap, shadowPass ? m_dummyTexture : m_shadowDepthTexture);
                Render::BindBuffer(bindings, drawBindings.instances, instances.handle, 0, instances.size);
                Render::BindBuffer(bindings, drawBindings.bones, boneBuffer, 0, boneBufferSize);
                Render::BindBuffer(bindings, drawBindings.lights, m_lightBuffer, 0, sizeof(LightGPU) * MAX_RENDER_LIGHTS);
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
        m_deferredPositionBinding = ResolveResourceBinding(m_deferredLightingInterface, "GBufferPosition");
        m_deferredLightsBinding = ResolveResourceBinding(m_deferredLightingInterface, "lights");
        m_deferredShadowBinding = ResolveResourceBinding(m_deferredLightingInterface, "shadowMap");

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
        desc.colorAttachmentFormats.push_back(RHI_Format::RGBA16_Float);
        desc.depthAttachmentFormat = RHI_Format::Unknown;
        m_deferredLightingPipeline = m_rhi->CreateGraphicsPipeline(desc);
        m_rhi->SetDebugName(m_deferredLightingVertexShader, "Renderer.DeferredLighting.VertexShader");
        m_rhi->SetDebugName(m_deferredLightingFragmentShader, "Renderer.DeferredLighting.FragmentShader");
        m_rhi->SetDebugName(m_deferredLightingPipeline, "Renderer.DeferredLighting.Pipeline");
    }

    /**
     * @brief 创建独立 Post Process Pipeline，使 HDR 到显示输出的转换不进入材质 Shader。
     */
    void RenderPipeline::CreatePostProcessResources()
    {
        const Asset::ShaderHandle vertexAsset = m_assetMgr->LoadShader("Assets/Shaders/fullscreen.vert");
        const Asset::ShaderHandle fragmentAsset = m_assetMgr->LoadShader("Assets/Shaders/post_process.frag");
        const auto* vertex = m_assetMgr->GetShader(vertexAsset);
        const auto* fragment = m_assetMgr->GetShader(fragmentAsset);
        if (!vertex || !fragment || !vertex->hasReflection || !fragment->hasReflection)
            return;

        const std::array stages{ vertex->reflection, fragment->reflection };
        Shader::ShaderProgramBuildResult interfaceResult = Shader::BuildShaderProgramInterface(stages);
        if (!interfaceResult.success)
        {
            for (const std::string& error : interfaceResult.errors)
                LOG_ERROR("Renderer", "Post process shader interface conflict: {}", error);
            return;
        }
        m_postProcessInterface = std::move(interfaceResult.interface);
        m_postProcessSceneColorBinding = ResolveResourceBinding(m_postProcessInterface, "HDRSceneColor");
        m_postProcessDataBinding = ResolveResourceBinding(m_postProcessInterface, "postProcess");
        m_postProcessVertexShader = m_rhi->CreateShader({ .stage = RHI_ShaderStage::Vertex, .code = vertex->spirv.data(), .codeSize = vertex->spirv.size() });
        m_postProcessFragmentShader = m_rhi->CreateShader({ .stage = RHI_ShaderStage::Fragment, .code = fragment->spirv.data(), .codeSize = fragment->spirv.size() });

        PipelineDesc desc{
            .vertexShader = m_postProcessVertexShader,
            .fragmentShader = m_postProcessFragmentShader,
            .shaderInterface = m_postProcessInterface,
            .vertexLayout = {},
            .depthTest = false,
            .depthWrite = false,
        };
        desc.colorAttachmentFormats.push_back(RHI_Format::RGBA8_UNorm);
        desc.depthAttachmentFormat = RHI_Format::Unknown;
        m_postProcessPipeline = m_rhi->CreateGraphicsPipeline(desc);
        m_rhi->SetDebugName(m_postProcessVertexShader, "Renderer.PostProcess.VertexShader");
        m_rhi->SetDebugName(m_postProcessFragmentShader, "Renderer.PostProcess.FragmentShader");
        m_rhi->SetDebugName(m_postProcessPipeline, "Renderer.PostProcess.Pipeline");
    }

    void RenderPipeline::BeginFrame()
    {
        CHIKA_PROFILE_SCOPE("RenderPipeline.BeginFrame");
        // 清空 Renderer 侧汇总，避免跳帧时继续显示上一帧统计。
        m_frameStatistics.Reset();
        HandlePendingResize();
    }

    void RenderPipeline::Execute(float deltaTime)
    {
        CHIKA_PROFILE_SCOPE("RenderPipeline.Execute");
        m_time += deltaTime;
        {
            CHIKA_PROFILE_SCOPE("RenderPipeline.UpdateFrameData");
            UpdateSceneDataFromSnapshot();
            PrepareLightData();
            UpdatePostProcessData();
        }
        {
            CHIKA_PROFILE_SCOPE("RenderPipeline.PrepareResources");
            PrepareSnapshotResources();
        }
        {
            CHIKA_PROFILE_SCOPE("RenderPipeline.PrepareQueues");
            PrepareRenderQueues();
        }
        {
            CHIKA_PROFILE_SCOPE("RenderPipeline.PrepareInstances");
            PrepareInstanceData();
        }

        {
            CHIKA_PROFILE_SCOPE("RenderPipeline.BuildGraph");
            BuildRenderGraph();
        }

        {
            CHIKA_PROFILE_SCOPE("RenderPipeline.ExecuteGraph");
            m_renderGraph->Execute();
        }

        // RHI 负责命令统计，RenderGraph 负责 Pass 统计；Renderer 在帧执行结束后汇总两者。
        m_frameStatistics = m_rhi->GetFrameStatistics();
        m_frameStatistics.passCount = m_renderGraph->GetLastExecutedPassCount();
        m_frameStatistics.visibleObjectCount = m_visibleObjectCount;
        m_frameStatistics.culledObjectCount = m_culledObjectCount;
        m_frameStatistics.staticOpaqueObjectCount = m_preparationDiagnostics.classification.staticOpaqueCount;
        m_frameStatistics.skinnedObjectCount = m_preparationDiagnostics.classification.skinnedCount;
        m_frameStatistics.transparentObjectCount = m_preparationDiagnostics.classification.transparentCount;
        m_frameStatistics.invalidResourceObjectCount = m_preparationDiagnostics.classification.invalidResourceCount;
        m_frameStatistics.preparationFallback = static_cast<uint32_t>(m_preparationDiagnostics.fallback);
        m_frameStatistics.validationHashVersion = kRenderValidationHashVersion;
        m_frameStatistics.visibleSetHash = m_preparationDiagnostics.hashes.visibleSet;
        m_frameStatistics.packetHash = m_preparationDiagnostics.hashes.packets;
        m_frameStatistics.batchHash = m_preparationDiagnostics.hashes.batches;
        m_frameStatistics.drawInputHash = m_preparationDiagnostics.hashes.drawInput;
        m_frameStatistics.preparationCpuTimeMs = m_preparationDiagnostics.totalCpuTimeMs;
        m_frameStatistics.sceneViewCpuTimeMs = m_preparationDiagnostics.sceneViewCpuTimeMs;
        m_frameStatistics.resourceViewCpuTimeMs = m_preparationDiagnostics.resourceViewCpuTimeMs;
        m_frameStatistics.visibilityCpuTimeMs = m_preparationDiagnostics.visibilityCpuTimeMs;
        m_frameStatistics.packetCpuTimeMs = m_preparationDiagnostics.packetCpuTimeMs;
        m_frameStatistics.sortCpuTimeMs = m_preparationDiagnostics.sortCpuTimeMs;
        m_frameStatistics.renderJobsUsed = m_preparationDiagnostics.jobsUsed;
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
        CHIKA_PROFILE_COUNTER("Renderer.PassCount", static_cast<int64_t>(m_frameStatistics.passCount));
        CHIKA_PROFILE_COUNTER("Renderer.DrawCalls", static_cast<int64_t>(m_frameStatistics.drawCallCount));
        CHIKA_PROFILE_COUNTER("Renderer.Instances", static_cast<int64_t>(m_frameStatistics.instanceCount));
        CHIKA_PROFILE_COUNTER("Renderer.VisibleObjects", static_cast<int64_t>(m_frameStatistics.visibleObjectCount));
        CHIKA_PROFILE_COUNTER("Renderer.CulledObjects", static_cast<int64_t>(m_frameStatistics.culledObjectCount));
        CHIKA_PROFILE_COUNTER("Renderer.Batches", static_cast<int64_t>(m_frameStatistics.batchCount));
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
        }
        sceneData->frameOptions[0] = m_settings->ambientIntensity;
        sceneData->frameOptions[1] = static_cast<float>(std::min<size_t>(m_snapshot->lights.size(), MAX_RENDER_LIGHTS));
        sceneData->frameOptions[2] = m_settings->shadows.depthBias;
        sceneData->frameOptions[3] = m_settings->shadows.normalBias;
        sceneData->shadowOptions[0] = 1.0f / static_cast<float>(m_settings->shadows.resolution);
        sceneData->shadowOptions[1] = sceneData->shadowOptions[0];
        sceneData->shadowOptions[2] = static_cast<float>(m_settings->shadows.pcfRadius);
        sceneData->shadowOptions[3] = 0.0f;
    }

    /**
     * @brief 把 RenderWorld Light Proxy 转换成后端无关的帧级 GPU Light Buffer。
     */
    void RenderPipeline::PrepareLightData()
    {
        auto* mapped = static_cast<LightGPU*>(m_rhi->GetMappedData(m_lightBuffer));
        if (!mapped)
            return;
        std::memset(mapped, 0, sizeof(LightGPU) * MAX_RENDER_LIGHTS);
        if (!m_snapshot)
            return;

        const size_t lightCount = std::min<size_t>(m_snapshot->lights.size(), MAX_RENDER_LIGHTS);
        for (size_t index = 0; index < lightCount; ++index)
        {
            const RenderLightProxy& source = m_snapshot->lights[index].proxy;
            const Math::Vector3 direction = source.direction.Normalized();
            LightGPU& target = mapped[index];
            target.positionRange[0] = source.position.x;
            target.positionRange[1] = source.position.y;
            target.positionRange[2] = source.position.z;
            target.positionRange[3] = source.range;
            target.directionType[0] = direction.x;
            target.directionType[1] = direction.y;
            target.directionType[2] = direction.z;
            target.directionType[3] = static_cast<float>(source.type);
            target.colorIntensity[0] = source.color.x;
            target.colorIntensity[1] = source.color.y;
            target.colorIntensity[2] = source.color.z;
            target.colorIntensity[3] = source.intensity;
            target.spotAngles[0] = source.innerConeCos;
            target.spotAngles[1] = source.outerConeCos;
            target.spotAngles[2] = source.castsShadow ? 1.0f : 0.0f;
        }
    }

    /**
     * @brief 将 RenderSettings 的后处理配置写入 Reflection 驱动的独立 Frame UBO。
     */
    void RenderPipeline::UpdatePostProcessData()
    {
        auto* data = static_cast<PostProcessData*>(m_rhi->GetMappedData(m_postProcessUBO));
        if (!data || !m_settings)
            return;
        const PostProcessSettings& settings = m_settings->postProcess;
        data->toneMapping[0] = settings.exposure;
        data->toneMapping[1] = settings.bloomThreshold;
        data->toneMapping[2] = settings.bloomIntensity;
        data->toneMapping[3] = settings.toneMappingEnabled ? 1.0f : 0.0f;
        data->imageOptions[0] = 1.0f / static_cast<float>(std::max(1u, m_viewportWidth));
        data->imageOptions[1] = 1.0f / static_cast<float>(std::max(1u, m_viewportHeight));
        data->imageOptions[2] = settings.bloomEnabled ? 1.0f : 0.0f;
        data->imageOptions[3] = settings.fxaaEnabled ? 1.0f : 0.0f;
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
        m_preparationDiagnostics = {};
        if (!m_snapshot)
            return;

        const RenderView* primaryView = m_snapshot->viewFamily.GetPrimaryView();
        if (!primaryView)
            return;

        RenderView shadowView;
        if (!m_snapshot->lights.empty())
        {
            const RenderLightProxy& light = m_snapshot->lights.front().proxy;
            shadowView = {
                .viewProjection = light.viewProjection,
                .layerMask = light.layerMask,
            };
        }
        else
            shadowView = *primaryView;

        const uint64_t resourceBegin = Profiler::ProfilerClock::NowNanoseconds();
        const RenderResourceView resourceView = RenderResourceView::Build(*m_snapshot, *m_resourceMgr);
        const double resourceViewCpuTimeMs = static_cast<double>(Profiler::ProfilerClock::NowNanoseconds() - resourceBegin) / 1'000'000.0;

        RenderPreparationResult preparation = PrepareRenderData(m_snapshot,
                                                                *primaryView,
                                                                shadowView,
                                                                resourceView,
                                                                m_jobSystem,
                                                                {
                                                                    .mode = m_settings->cpuMode,
                                                                    .minimumObjects = m_settings->parallelObjectThreshold,
                                                                    .visibilityGrainSize = m_settings->visibilityGrainSize,
                                                                    .packetGrainSize = m_settings->packetGrainSize,
                                                                    .sortMinimumPackets = m_settings->parallelSortThreshold,
                                                                    .sortGrainSize = m_settings->sortGrainSize,
                                                                });
        m_visibleObjectCount = preparation.mainVisibility.visibleObjectCount;
        m_culledObjectCount = preparation.mainVisibility.culledObjectCount;
        m_renderQueues = std::move(preparation.queues);
        m_preparationDiagnostics = preparation.diagnostics;
        m_preparationDiagnostics.resourceViewCpuTimeMs = resourceViewCpuTimeMs;
        m_preparationDiagnostics.totalCpuTimeMs += resourceViewCpuTimeMs;
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

        m_overlayCallback = {};
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
        m_jobSystem = nullptr;
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
        destroyPipeline(m_postProcessPipeline);
        destroyShader(m_deferredLightingVertexShader);
        destroyShader(m_deferredLightingFragmentShader);
        destroyShader(m_postProcessVertexShader);
        destroyShader(m_postProcessFragmentShader);
        destroyBuffer(m_sceneUBO);
        destroyBuffer(m_lightBuffer);
        destroyBuffer(m_postProcessUBO);
        destroyBuffer(m_dummyBoneUBO);
        destroyTexture(m_offscreenColor);
        destroyTexture(m_hdrSceneColor);
        destroyTexture(m_depthTexture);
        destroyTexture(m_dummyTexture);
        destroyTexture(m_shadowDepthTexture);
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
        m_rhi->DestroyTexture(m_hdrSceneColor);
        m_rhi->DestroyTexture(m_depthTexture);

        // 重建 Color
        TextureDesc colorDesc{
            .width = m_viewportWidth,
            .height = m_viewportHeight,
            .format = Render::RHI_Format::RGBA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_offscreenColor = m_rhi->CreateTexture(colorDesc);
        m_rhi->SetDebugName(m_offscreenColor, "Renderer.OffscreenColor");
        colorDesc.format = Render::RHI_Format::RGBA16_Float;
        m_hdrSceneColor = m_rhi->CreateTexture(colorDesc);
        m_rhi->SetDebugName(m_hdrSceneColor, "Renderer.HDRSceneColor");

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

    const RenderGraphDebugSnapshot& RenderPipeline::GetRenderGraphDebugSnapshot() const
    {
        return m_renderGraph->GetDebugSnapshot();
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
