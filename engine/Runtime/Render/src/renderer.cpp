#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RenderPassBuilder.hpp"
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

        RHI_InitParams params{
            .nativeWindowHandle = createInfo.windowHandle,
            .width = createInfo.width,
            .height = createInfo.height,
            .enableValidation = true,
        };

        m_rhi = std::make_unique<VulkanRHIDevice>(params);
        m_renderGraph = std::make_unique<RenderGraph>(m_rhi.get());
        m_resourceMgr = std::make_unique<Resource::ResourceManager>(*m_rhi, m_assetMgr);

        m_meshAsset = m_assetMgr.LoadMesh("Assets/Meshes/cube.obj");
        m_floorMatAsset = m_assetMgr.LoadMaterial("Assets/Materials/test.json");
        m_cubeMatAsset = m_assetMgr.LoadMaterial("Assets/Materials/test01.json");

        m_meshGPU = m_resourceMgr->UploadMesh(m_meshAsset);
        m_floorMatGPU = m_resourceMgr->UploadMaterial(m_floorMatAsset);
        m_cubeMatGPU = m_resourceMgr->UploadMaterial(m_cubeMatAsset);

        // m_matGPU = m_resourceMgr->UploadMaterial(m_matAsset);

        // TextureDesc colorDesc{};
        // colorDesc.width = m_width;
        // colorDesc.height = m_height;
        // colorDesc.format = RHI_Format::RGBA8_UNorm;
        // m_offscreenColor = m_rhi->CreateTexture(colorDesc);

        // 场景 UBO 创建
        BufferDesc uboDesc{
            .size = sizeof(SceneData),
            .usage = Render::RHI_BufferUsage::Uniform,
            .memoryUsage = Render::MemoryUsage::CPU_To_GPU,
        };
        m_sceneUBO = m_rhi->CreateBuffer(uboDesc);

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
        TextureDesc depthDesc{
            .width = m_width,
            .height = m_height,
            .format = Render::RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = Render::RHI_TextureUsage::DepthStencilAttachment,
        };
        m_depthTexture = m_rhi->CreateTexture(depthDesc);
        m_rgDepth = m_renderGraph->ImportTexture("Depth", m_depthTexture, depthDesc);

        // Shadow
        Render::TextureDesc shadowDepthDesc{
            .width = 2048, // 阴影分辨率稍微给高点
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
        // BuildRenderGraph();
    }

    void Renderer::BuildRenderGraph()
    {
        m_renderGraph->Clear();
        Render::TextureDesc depthDesc{
            .width = m_width,
            .height = m_height,
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

        m_renderGraph->AddPresentPass("Present", m_rgSwapchain);

        m_renderGraph->Compile();
    }
    void Renderer::AddUploadPasses()
    {
        auto bufferJobs = m_resourceMgr->GetBufferUploadJobs();
        auto textureJobs = m_resourceMgr->GetTextureUploadJobs();

        if (bufferJobs.empty() && textureJobs.empty())
            return;

        m_renderGraph->AddUploadPass("Upload Resources",
                                     [bufferJobs, textureJobs](IRHICommandList* cmd, RenderGraph* graph)
                                     {
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
                const auto& renderMesh = m_resourceMgr->GetMesh(m_meshGPU);
                const auto& cubeMat = m_resourceMgr->GetMaterial(m_cubeMatGPU);
                const auto& floorMat = m_resourceMgr->GetMaterial(m_floorMatGPU);

                cmd->BindVertexBuffer(renderMesh.vertexBuffer, 0);
                cmd->BindIndexBuffer(renderMesh.indexBuffer, 0, renderMesh.isUint32);

                using namespace ChikaEngine::Math;
                PC pc;
                pc.isShadowPass = 1;

                // Cube
                Mat4 cubeModel = Mat4::Identity();
                cubeModel.Translate(Vector3(0.0f, 1.0f, 0.0f));
                cubeModel.RotateY(m_time);
                pc.model = cubeModel.Transposed();

                cmd->BindPipeline(cubeMat.pipeline);
                auto cubeBindings = cubeMat.bindings;
                cubeBindings.BindBuffer(0, m_sceneUBO, 0, sizeof(SceneData));
                cubeBindings.BindTexture(4, m_dummyTexture);
                cmd->BindResources(0, cubeBindings);
                cmd->PushConstants(sizeof(PC), &pc);
                cmd->DrawIndexed(renderMesh.indexCount, 1);

                // Floor
                Mat4 floorModel = Mat4::Identity();
                floorModel.Scale(Vector3(10.0f, 0.1f, 10.0f));
                floorModel.Translate(Vector3(0.0f, -2.0f, 0.0f));
                pc.model = floorModel.Transposed();

                cmd->BindPipeline(floorMat.pipeline);
                auto floorBindings = floorMat.bindings;
                floorBindings.BindBuffer(0, m_sceneUBO, 0, sizeof(SceneData));
                floorBindings.BindTexture(4, m_dummyTexture);
                cmd->BindResources(0, floorBindings);
                cmd->PushConstants(sizeof(PC), &pc);
                cmd->DrawIndexed(renderMesh.indexCount, 1);
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
                builder.WriteColor(m_rgSwapchain, LoadOp::Clear, clearColor);
                builder.WriteDepth(m_rgDepth, LoadOp::Clear);
            },
            [this](IRHICommandList* cmd, RenderGraph* graph)
            {
                const auto& renderMesh = m_resourceMgr->GetMesh(m_meshGPU);
                const auto& cubeMat = m_resourceMgr->GetMaterial(m_cubeMatGPU);
                const auto& floorMat = m_resourceMgr->GetMaterial(m_floorMatGPU);

                cmd->BindVertexBuffer(renderMesh.vertexBuffer, 0);
                cmd->BindIndexBuffer(renderMesh.indexBuffer, 0, renderMesh.isUint32);

                using namespace ChikaEngine::Math;
                PC pc;
                pc.isShadowPass = 0;

                Mat4 cubeModel = Mat4::Identity();
                cubeModel.Translate(Vector3(0.0f, 1.0f, 0.0f));
                cubeModel.RotateY(m_time);
                pc.model = cubeModel.Transposed();

                cmd->BindPipeline(cubeMat.pipeline);
                auto cubeBindings = cubeMat.bindings;
                cubeBindings.BindBuffer(0, m_sceneUBO, 0, sizeof(SceneData));
                cubeBindings.BindTexture(4, m_shadowDepthTexture); // 正式绑定渲染好的阴影图
                cmd->BindResources(0, cubeBindings);
                cmd->PushConstants(sizeof(PC), &pc);
                cmd->DrawIndexed(renderMesh.indexCount, 1);


                Mat4 floorModel = Mat4::Identity();
                floorModel.Scale(Vector3(10.0f, 0.1f, 10.0f));
                floorModel.Translate(Vector3(0.0f, -2.0f, 0.0f));
                pc.model = floorModel.Transposed();

                cmd->BindPipeline(floorMat.pipeline);
                auto floorBindings = floorMat.bindings;
                floorBindings.BindBuffer(0, m_sceneUBO, 0, sizeof(SceneData));
                floorBindings.BindTexture(4, m_shadowDepthTexture);
                cmd->BindResources(0, floorBindings);
                cmd->PushConstants(sizeof(PC), &pc);
                cmd->DrawIndexed(renderMesh.indexCount, 1);
            });
    }
    void Renderer::BeginFrame()
    {
        m_rhi->BeginFrame();
    }

    void Renderer::Tick(float deltaTime)
    {
        m_time += deltaTime;

        using namespace ChikaEngine::Math;

        Vector3 lightPos(5.0f, 8.0f, 5.0f);
        Mat4 lightView = Mat4::LookAt(lightPos, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
        Mat4 lightProj = Mat4::Perspective(3.1415926f / 3.0f, 1.0f, 0.1f, 100.0f);
        lightProj(1, 1) *= -1.0f;
        lightProj(2, 2) = -100.0f / (100.0f - 0.1f);
        lightProj(2, 3) = -(100.0f * 0.1f) / (100.0f - 0.1f);
        Mat4 lightVP = lightProj * lightView;

        Vector3 camPos(0.0f, 4.0f, 8.0f);
        Mat4 cameraView = Mat4::LookAt(camPos, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
        Mat4 cameraProj = Mat4::Perspective(3.1415926f / 4.0f, float(m_width) / float(m_height), 0.1f, 100.0f);
        cameraProj(1, 1) *= -1.0f;
        cameraProj(2, 2) = -100.0f / (100.0f - 0.1f);
        cameraProj(2, 3) = -(100.0f * 0.1f) / (100.0f - 0.1f);
        Mat4 cameraVP = cameraProj * cameraView;

        SceneData* sceneData = static_cast<SceneData*>(m_rhi->GetMappedData(m_sceneUBO));
        if (sceneData)
        {
            sceneData->cameraVP = cameraVP.Transposed();
            sceneData->lightVP = lightVP.Transposed();
            // 平行光方向即从 lightPos 指向原点
            sceneData->lightDir[0] = lightPos.x;
            sceneData->lightDir[1] = lightPos.y;
            sceneData->lightDir[2] = lightPos.z;
            sceneData->lightDir[3] = 0.0f;
            sceneData->viewPos[0] = camPos.x;
            sceneData->viewPos[1] = camPos.y;
            sceneData->viewPos[2] = camPos.z;
            sceneData->viewPos[3] = 1.0f;
        }

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

    void Renderer::Shutdown()
    {
        m_renderGraph->Clear();
        m_rhi->DestroyTexture(m_offscreenColor);
    }
} // namespace ChikaEngine::Render