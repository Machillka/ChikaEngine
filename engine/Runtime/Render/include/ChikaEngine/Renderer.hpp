/*!
 * @file Renderer.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 组装 renderer 上层调用接口提供
 * @version 0.1
 * @date 2026-03-26
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/AssetHandle.hpp"
#include "ChikaEngine/ResourceHandle.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include "IRHICommandList.hpp"
#include "IRHIDevice.hpp"
#include "RHIResourceHandle.hpp"
#include "RenderGraph.hpp"
#include <cstdint>
#include <memory>
#include "ChikaEngine/math/mat4.h"

namespace ChikaEngine::Render
{
    // ubo 临时定义
    struct SceneData
    {
        Math::Mat4 cameraVP;
        Math::Mat4 lightVP;
        float lightDir[4];
        float viewPos[4];
    };

    // push constants 临时定义
    struct PC
    {
        Math::Mat4 model;
        int isShadowPass;
        int padding[3];
    };

    struct RendererCreateInfo
    {
        void* windowHandle = nullptr;
        uint32_t width = 1920;
        uint32_t height = 1080;
    };

    class Renderer
    {
      public:
        void Initialize(const RendererCreateInfo& createInfo);
        void Shutdown();
        void BeginFrame();
        void Tick(float deltaTimt); // 渲染逻辑 Tick
        void EndFrame();

      public:
        IRHIDevice* GetRHIHandle() const;

      private:
        // 写死 Imgui 的使用逻辑
        void SetupImgui();
        void BuildRenderGraph();
        void RecordImGuiPass(IRHICommandList* cmd);

      private:
        void AddMainScenePass();
        void AddUploadPasses();
        void AddShadowPass();

      private:
        void* m_window = nullptr;
        std::unique_ptr<IRHIDevice> m_rhi = nullptr;
        std::unique_ptr<RenderGraph> m_renderGraph = nullptr;

        uint32_t m_width = 1920;
        uint32_t m_height = 1080;
        TextureHandle m_offscreenColor;

        std::unique_ptr<Resource::ResourceManager> m_resourceMgr;
        Asset::AssetManager m_assetMgr;

        Asset::MeshHandle m_meshAsset;
        Asset::MeshHandle m_objAsset;

        // Asset::MaterialHandle m_matAsset;
        Asset::MaterialHandle m_cubeMatAsset;
        Asset::MaterialHandle m_floorMatAsset;

        Resource::MaterialHandle m_floorMatGPU;
        Resource::MaterialHandle m_cubeMatGPU;

        Resource::MeshHandle m_meshGPU;
        Resource::MeshHandle m_objGPU;

        TextureHandle m_depthTexture;
        RGTextureHandle m_rgSwapchain;
        RGTextureHandle m_rgDepth;

        // shadow
        TextureHandle m_shadowDepthTexture;
        TextureHandle m_shadowColorTexture; // TODO: add pass 提供不依赖 color attachment 的接口
        RGTextureHandle m_rgShadowDepth;
        RGTextureHandle m_rgShadowColor;

        float m_time = 0.0f;

        BufferHandle m_sceneUBO;
        TextureHandle m_dummyTexture;
    };

} // namespace ChikaEngine::Render