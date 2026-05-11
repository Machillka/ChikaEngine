#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RenderPassBuilder.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/math/mat4.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/rhi/Vulkan/VulkanRHIDevice.hpp"
#include <memory>

namespace ChikaEngine::Render
{
    void Renderer::Initialize(const RendererCreateInfo& createInfo)
    {
        m_window = createInfo.windowHandle;
        m_width = createInfo.width;
        m_height = createInfo.height;
        m_assetMgr = createInfo.assetManager;

        m_viewportWidth = m_width;
        m_viewportHeight = m_height;

        m_defaultCamera = std::make_unique<Camera>();
        // 设置纵横比
        m_defaultCamera->SetPerspective(45.0f, GetViewportAspectRatio(), 0.1f, 1000.0f);

        // 设置初始位置和朝向
        m_defaultCamera->SetPosition(Math::Vector3(0.0f, 4.0f, 8.0f));
        m_defaultCamera->SetLookAt(Math::Vector3(0.0f, 0.0f, 0.0f));

        // 初始状态下指向默认摄像机
        m_activeCamera = m_defaultCamera.get();

        RHI_InitParams params{
            .nativeWindowHandle = createInfo.windowHandle,
            .width = createInfo.width,
            .height = createInfo.height,
            .enableValidation = true,
        };

        m_rhi = std::make_unique<VulkanRHIDevice>(params);
        m_renderGraph = std::make_unique<RenderGraph>(m_rhi.get());
        m_resourceMgr = std::make_unique<Resource::ResourceManager>(*m_rhi, *m_assetMgr);

        // 场景 UBO 创建
        BufferDesc uboDesc{
            .size = sizeof(SceneData),
            .usage = Render::RHI_BufferUsage::Uniform,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };
        m_sceneUBO = m_rhi->CreateBuffer(uboDesc);

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
            .size = 128 * sizeof(Math::Mat4),
            .usage = Render::RHI_BufferUsage::Uniform,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };
        m_dummyBoneUBO = m_rhi->CreateBuffer(dummyBoneDesc);

        // 填充空数据
        Math::Mat4* mappedBones = static_cast<Math::Mat4*>(m_rhi->GetMappedData(m_dummyBoneUBO));
        if (mappedBones)
        {
            for (int i = 0; i < 128; ++i)
            {
                mappedBones[i] = Math::Mat4::Identity();
            }
        }

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
        m_depthTexture = m_rhi->CreateTexture(depthDesc);
        m_rgDepth = m_renderGraph->ImportTexture("Depth", m_depthTexture, depthDesc);

        // Shadow
        Render::TextureDesc shadowDepthDesc{
            .width = 2048,
            .height = 2048,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_shadowDepthTexture = m_rhi->CreateTexture(shadowDepthDesc);
        m_rgShadowDepth = m_renderGraph->ImportTexture("ShadowDepth", m_shadowDepthTexture, shadowDepthDesc);

        Render::TextureDesc shadowColorDesc{
            .width = 2048,
            .height = 2048,
            .format = Render::RHI_Format::BGRA8_UNorm,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::ColorAttachment,
        };
        m_shadowColorTexture = m_rhi->CreateTexture(shadowColorDesc);
        m_rgShadowColor = m_renderGraph->ImportTexture("ShadowColorDummy", m_shadowColorTexture, shadowColorDesc);

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

    void Renderer::SubmitDrawCommands(const std::vector<DrawCommand>& drawCommandQueue)
    {
        m_drawCommandQueue = drawCommandQueue;
    }

    void Renderer::BuildRenderGraph()
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
            .width = 2048,
            .height = 2048,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment | Render::RHI_TextureUsage::Sampled,
        };
        m_rgShadowDepth = m_renderGraph->ImportTexture("ShadowDepth", m_shadowDepthTexture, shadowDepthDesc);

        Render::TextureDesc shadowColorDesc{
            .width = 2048,
            .height = 2048,
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
        AddMainScenePass();
        AddImGuiPass();

        m_renderGraph->AddPresentPass("Present", m_rgSwapchain);

        m_renderGraph->Compile();
    }

    void Renderer::AddUploadPasses()
    {
        auto bufferJobs = m_resourceMgr->GetBufferUploadJobs();
        auto textureJobs = m_resourceMgr->GetTextureUploadJobs();

        static bool s_dummyTextureTransitioned = false;

        if (bufferJobs.empty() && textureJobs.empty() && s_dummyTextureTransitioned)
            return;

        m_renderGraph->AddUploadPass("Upload Resources",
                                     [this, bufferJobs, textureJobs](IRHICommandList* cmd, RenderGraph* graph)
                                     {
                                         if (!s_dummyTextureTransitioned)
                                         {
                                             cmd->InsertTextureBarrier(m_dummyTexture, ResourceState::Undefined, ResourceState::ShaderResource);
                                             s_dummyTextureTransitioned = true;
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

    void Renderer::AddShadowPass()
    {
        m_renderGraph->AddPass(
            "Shadow Pass",
            [&](RGPassBuilder& builder)
            {
                const float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                builder.WriteColor(m_rgShadowColor, LoadOp::Clear, clearColor);
                builder.WriteDepth(m_rgShadowDepth, LoadOp::Clear);
            },
            [this](IRHICommandList* cmd, RenderGraph* graph)
            {
                using namespace ChikaEngine::Math;
                PC pc;
                pc.isShadowPass = 1;

                SceneData* sceneData = static_cast<SceneData*>(m_rhi->GetMappedData(m_sceneUBO));
                if (m_activeCamera)
                {
                    sceneData->cameraVP = m_activeCamera->GetViewProjectionMatrix().Transposed();

                    auto pos = m_activeCamera->GetPosition();
                    sceneData->viewPos[0] = pos.x;
                    sceneData->viewPos[1] = pos.y;
                    sceneData->viewPos[2] = pos.z;
                    sceneData->viewPos[3] = 1.0f;
                }

                for (const auto& drawCmd : m_drawCommandQueue)
                {
                    if (!drawCmd.meshHandle.IsValid() || !drawCmd.materialHandle.IsValid())
                        continue;

                    const auto& mesh = m_resourceMgr->GetMesh(drawCmd.meshHandle);
                    const auto& material = m_resourceMgr->GetMaterial(drawCmd.materialHandle);

                    if (!material.pipeline.IsValid() || !mesh.vertexBuffer.IsValid())
                        continue;

                    cmd->BindVertexBuffer(mesh.vertexBuffer, 0);
                    cmd->BindIndexBuffer(mesh.indexBuffer, 0, mesh.isUint32);

                    cmd->BindPipeline(material.pipeline);
                    auto bindings = material.bindings;
                    bindings.BindBuffer(1, m_sceneUBO, 0, sizeof(SceneData));
                    bindings.BindTexture(4, m_dummyTexture);

                    if (drawCmd.isSkinned && drawCmd.boneUBO.IsValid())
                    {
                        bindings.BindBuffer(2, drawCmd.boneUBO, 0, 128 * sizeof(Math::Mat4));
                        pc.isSkinned = 1;
                    }
                    else
                    {
                        bindings.BindBuffer(2, m_dummyBoneUBO, 0, 128 * sizeof(Math::Mat4));
                        pc.isSkinned = 0;
                    }

                    pc.model = drawCmd.model.Transposed();

                    cmd->BindResources(0, bindings);
                    cmd->PushConstants(sizeof(PC), &pc);
                    cmd->DrawIndexed(mesh.indexCount, 1);
                }
            });
    }

    void Renderer::AddMainScenePass()
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
                using namespace ChikaEngine::Math;
                PC pc;
                pc.isShadowPass = 0;

                for (const auto& drawCmd : m_drawCommandQueue)
                {
                    if (!drawCmd.meshHandle.IsValid() || !drawCmd.materialHandle.IsValid())
                        continue;
                    const auto& mesh = m_resourceMgr->GetMesh(drawCmd.meshHandle);
                    const auto& material = m_resourceMgr->GetMaterial(drawCmd.materialHandle);
                    if (!material.pipeline.IsValid() || !mesh.vertexBuffer.IsValid())
                        continue;
                    cmd->BindVertexBuffer(mesh.vertexBuffer, 0);
                    cmd->BindIndexBuffer(mesh.indexBuffer, 0, mesh.isUint32);
                    cmd->BindPipeline(material.pipeline);

                    auto bindings = material.bindings;
                    bindings.BindBuffer(1, m_sceneUBO, 0, sizeof(SceneData));
                    bindings.BindTexture(4, m_shadowDepthTexture);

                    // 骨骼绑到 slot 2 上
                    if (drawCmd.isSkinned && drawCmd.boneUBO.IsValid())
                    {
                        bindings.BindBuffer(2, drawCmd.boneUBO, 0, 128 * sizeof(Math::Mat4));
                        pc.isSkinned = 1;
                    }
                    else
                    {
                        bindings.BindBuffer(2, m_dummyBoneUBO, 0, 128 * sizeof(Math::Mat4));
                        pc.isSkinned = 0;
                    }

                    pc.model = drawCmd.model.Transposed();

                    cmd->BindResources(0, bindings);

                    cmd->PushConstants(sizeof(PC), &pc);
                    cmd->DrawIndexed(mesh.indexCount, 1);
                }
            });
    }
    void Renderer::AddImGuiPass()
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
    void Renderer::BeginFrame()
    {
        HandlePendingResize();
        if (m_activeCamera)
        {
            m_activeCamera->SetAspectRatio(GetViewportAspectRatio());
        }
        m_rhi->BeginFrame();
    }

    void Renderer::Tick(float deltaTime)
    {
        m_time += deltaTime;

        BuildRenderGraph();

        m_renderGraph->Execute();
    }

    void Renderer::EndFrame()
    {
        m_rhi->EndFrame();
    }

    IRHIDevice* Renderer::GetRHIHandle() const
    {
        return m_rhi.get();
    }

    void Renderer::SubmitLight(Math::Mat4& lightMat, Math::Vector3 lightPos)
    {
        SceneData* sceneData = static_cast<SceneData*>(m_rhi->GetMappedData(m_sceneUBO));

        if (sceneData)
        {
            sceneData->lightVP = lightMat.Transposed();

            sceneData->lightDir[0] = lightPos.x;
            sceneData->lightDir[1] = lightPos.y;
            sceneData->lightDir[2] = lightPos.z;
            sceneData->lightDir[3] = 0.0f;
        }
    }
    // void Renderer::SubmitCamera(Math::Mat4& cameraMat, Math::Vector3 cameraPos)
    // {
    //     SceneData* sceneData = static_cast<SceneData*>(m_rhi->GetMappedData(m_sceneUBO));
    //     if (sceneData)
    //     {
    //         sceneData->cameraVP = cameraMat.Transposed();

    //         sceneData->viewPos[0] = cameraPos.x;
    //         sceneData->viewPos[1] = cameraPos.y;
    //         sceneData->viewPos[2] = cameraPos.z;
    //         sceneData->viewPos[3] = 1.0f;
    //     }
    // }

    Asset::AssetManager* Renderer::GetAssetManager()
    {
        return m_assetMgr;
    }

    Resource::ResourceManager* Renderer::GetResourceManager()
    {
        return m_resourceMgr.get();
    }

    void Renderer::Shutdown()
    {

        if (m_rhi)
        {
            m_rhi->WaitIdle();
        }

        m_renderGraph->Clear();
        m_rhi->DestroyTexture(m_offscreenColor);
    }

    /*!
     * @brief  提供 resize 的标记, 这样在一次 drawcall 之间多次调用只会记录最后一次数据
     *
     * @param  width
     * @param  height
     * @author Machillka (machillka2007@gmail.com)
     * @date 2026-04-26
     */
    // void Renderer::RequestResize(uint32_t width, uint32_t height)
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

    void Renderer::HandlePendingResize()
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

        // 通知 RenderGraph 更新物理句柄
        m_renderGraph->UpdateImportedTexture(m_rgOffscreen, m_offscreenColor);
        m_renderGraph->UpdateImportedTexture(m_rgDepth, m_depthTexture);

        LOG_INFO("Renderer", "Successfully resized to {}x{}", m_viewportWidth, m_viewportHeight);
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return; // 忽略最小化

        if (m_width == width && m_height == height)
            return;

        LOG_INFO("Renderer", "Main Window Resizing to {}x{}", width, height);

        // 必须等待 GPU 完成当前帧，才能销毁 Swapchain 相关的视图
        if (m_rhi)
        {
            m_rhi->WaitIdle();
            m_rhi->Resize(width, height); // 底层 RHI 重建 Swapchain
        }

        m_width = width;
        m_height = height;
    }

    void Renderer::OnViewResize(uint32_t width, uint32_t height)
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

} // namespace ChikaEngine::Render