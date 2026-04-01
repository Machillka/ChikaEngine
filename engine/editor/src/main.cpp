// #include "ChikaEngine/AssetManager.hpp"
// #include "ChikaEngine/RHIDesc.hpp"
// #include "ChikaEngine/Renderer.hpp"
// #include "ChikaEngine/ResourceLayout.hpp"
// #include "ChikaEngine/ResourceManager.hpp"
// #include "ChikaEngine/debug/log_system.h"
// #include "ChikaEngine/debug/console_sink.h"
// #include "ChikaEngine/math/mat4.h"
// #include "ChikaEngine/math/vector3.h"
// #include "glfw/glfw3.h"
// #include <iostream>
// #include <memory>
// int main()
// {
//     ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());
//     try
//     {
//         using namespace ChikaEngine;
//         if (!glfwInit())
//         {
//             throw std::runtime_error("Failed to initialize GLFW");
//         }
//         glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//         GLFWwindow* window = glfwCreateWindow(1280, 720, "Chika Engine - Pure Renderer Test", nullptr, nullptr);

//         auto render = std::make_unique<ChikaEngine::Render::Renderer>();
//         ChikaEngine::Render::RendererCreateInfo info{ .windowHandle = window, .width = 1280, .height = 720 };
//         render->Initialize(info);
//         auto rhi = render->GetRHIHandle();
//         Asset::AssetManager assetMgr;
//         Resource::ResourceManager resourceMgr(*rhi, assetMgr);

//         Asset::MeshHandle meshAsset = assetMgr.LoadMesh("Assets/Meshes/cube.obj");
//         Asset::MaterialHandle matAsset = assetMgr.LoadMaterial("Assets/Materials/test.json");

//         Render::TextureDesc depthDesc{ 1280, 720, Render::RHI_Format::D32_SFloat, 1, 1, Render::RHI_TextureUsage::DepthStencilAttachment };
//         Render::TextureHandle depthTexture = rhi->CreateTexture(depthDesc);

//         float time = 0.0f;
//         while (!glfwWindowShouldClose(window))
//         {
//             glfwPollEvents();
//             time += 0.016f;

//             render->BeginFrame();
//             Render::TextureHandle backbuffer = rhi->GetActiveSwapchainTexture();
//             Render::IRHICommandList* cmd = rhi->AllocateCommandList();
//             cmd->Begin();

//             Resource::MeshHandle rMeshH = resourceMgr.UploadMesh(meshAsset);
//             Resource::MaterialHandle rMatH = resourceMgr.UploadMaterial(matAsset);

//             const Resource::MeshGPU& renderMesh = resourceMgr.GetMesh(rMeshH);
//             const Resource::MaterialGPU& renderMat = resourceMgr.GetMaterial(rMatH);

//             cmd->InsertTextureBarrier(backbuffer, Render::ResourceState::Undefined, Render::ResourceState::RenderTarget);
//             cmd->InsertTextureBarrier(depthTexture, Render::ResourceState::Undefined, Render::ResourceState::DepthWrite);

//             Render::RenderingAttachment colorAtt{ backbuffer, Render::LoadOp::Clear, Render::StoreOp::Store, { 0.1f, 0.2f, 0.3f, 1.0f } };
//             Render::RenderingAttachment depthAtt{ depthTexture, Render::LoadOp::Clear, Render::StoreOp::Store, { 1.0f, 0.0f, 0.0f, 0.0f } };
//             std::vector<Render::RenderingAttachment> colors = { colorAtt };

//             cmd->BeginRendering(colors, &depthAtt);

//             cmd->BindPipeline(renderMat.pipeline);
//             cmd->BindResources(0, renderMat.bindings);

//             Math::Mat4 model = Math::Mat4::Identity();
//             model.RotateY(time);

//             Math::Mat4 view = Math::Mat4::LookAt(Math::Vector3(0.0f, 2.0f, 5.0f), Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(0.0f, 1.0f, 0.0f));

//             float zNear = 0.1f;
//             float zFar = 100.0f;
//             Math::Mat4 proj = Math::Mat4::Perspective(3.1415926f / 4.0f, 1280.0f / 720.0f, zNear, zFar);

//             proj(1, 1) *= -1.0f;
//             proj(2, 2) = -zFar / (zFar - zNear);
//             proj(2, 3) = -(zFar * zNear) / (zFar - zNear);

//             Math::Mat4 mvp = proj * view * model;

//             Math::Mat4 mvp_vulkan = mvp.Transposed();
//             // Math::Mat4 mvp_vulkan = Math::Mat4::Identity();
//             cmd->PushConstants(sizeof(Math::Mat4), &mvp_vulkan(0, 0));

//             cmd->BindVertexBuffer(renderMesh.vertexBuffer, 0);
//             cmd->BindIndexBuffer(renderMesh.indexBuffer, 0, renderMesh.isUint32);
//             cmd->DrawIndexed(renderMesh.indexCount, 1);

//             cmd->EndRendering();

//             // 转换为呈现状态上屏
//             cmd->InsertTextureBarrier(backbuffer, Render::ResourceState::RenderTarget, Render::ResourceState::Present);

//             cmd->End();
//             rhi->Submit(cmd);

//             render->EndFrame();
//         }

//         glfwDestroyWindow(window);
//         glfwTerminate();
//     }
//     catch (const std::exception& e)
//     {
//         std::cerr << "Fatal error: " << e.what() << std::endl;
//         return -1;
//     }
// }
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/debug/log_system.h"
#include "ChikaEngine/debug/console_sink.h"
#include "glfw/glfw3.h"
#include <iostream>
#include <memory>
int main()
{
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());
    try
    {
        using namespace ChikaEngine;
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(1280, 720, "Chika Engine - Pure Renderer Test", nullptr, nullptr);

        auto render = std::make_unique<ChikaEngine::Render::Renderer>();
        ChikaEngine::Render::RendererCreateInfo info{ .windowHandle = window, .width = 1280, .height = 720 };
        render->Initialize(info);

        double lastTime = glfwGetTime();
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            double now = glfwGetTime();
            float dt = float(now - lastTime);
            lastTime = now;

            render->BeginFrame();
            render->Tick(dt);
            render->EndFrame();
        }

        glfwDestroyWindow(window);
        glfwTerminate();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
}