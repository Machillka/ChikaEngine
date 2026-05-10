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

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/ResourceHandle.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include "IRHICommandList.hpp"
#include "IRHIDevice.hpp"
#include "RHIResourceHandle.hpp"
#include "RenderGraph.hpp"
#include <cstdint>
#include <memory>
#include <vector>
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
        int isSkinned;
    };

    struct RendererCreateInfo
    {
        void* windowHandle = nullptr;
        Asset::AssetManager* assetManager;
        uint32_t width = 1920;
        uint32_t height = 1080;
    };

    struct DrawCommand
    {
        Math::Mat4 model;
        Resource::MaterialHandle materialHandle;
        Resource::MeshHandle meshHandle;
        bool isSkinned = false;
        Render::BufferHandle boneUBO;
    };

    class Renderer
    {
      public:
        // TODO[x]: asset manager 应当是依赖注入
        void Initialize(const RendererCreateInfo& createInfo);
        void Shutdown();
        void BeginFrame();
        void Tick(float deltaTimt); // 渲染逻辑 Tick
        void EndFrame();

      public:
        void SubmitDrawCommands(const std::vector<DrawCommand>& drawCommandQueue);

        // 先使用单全局光
        void SubmitLight(Math::Mat4& lightMat, Math::Vector3 lightPos);
        void SubmitCamera(Math::Mat4& cameraMat, Math::Vector3 cameraPos);

      public:
        Asset::AssetManager* GetAssetManager();
        Resource::ResourceManager* GetResourceManager();

      public:
        IRHIDevice* GetRHIHandle() const;

      public:
        void SubmitImGuiData(void* drawData)
        {
            m_imguiDrawData = drawData;
        }
        void RequestResize(uint32_t width, uint32_t height);
        uint32_t GetViewportWidth() const
        {
            return m_viewportWidth;
        }
        uint32_t GetViewportHeight() const
        {
            return m_viewportHeight;
        }
        // 写死 Imgui 的使用逻辑
        TextureHandle GetOffscreenTexture() const
        {
            return m_offscreenColor;
        }
      private:

        void BuildRenderGraph();

      private:
        void AddMainScenePass();
        void AddUploadPasses();
        void AddShadowPass();
        void HandlePendingResize();
        void AddImGuiPass();

      private:
        std::vector<DrawCommand> m_drawCommandQueue;

      private:
        // 视口尺寸状态
        uint32_t m_viewportWidth = 1920;
        uint32_t m_viewportHeight = 1080;
        bool m_isResizePending = false;
        uint32_t m_pendingWidth = 1920;
        uint32_t m_pendingHeight = 1080;

        void* m_imguiDrawData = nullptr;

      private:
        void* m_window = nullptr;
        std::unique_ptr<IRHIDevice> m_rhi = nullptr;
        std::unique_ptr<RenderGraph> m_renderGraph = nullptr;

        uint32_t m_width = 1920;
        uint32_t m_height = 1080;
        TextureHandle m_offscreenColor;
        RGTextureHandle m_rgOffscreen;

        std::unique_ptr<Resource::ResourceManager> m_resourceMgr = nullptr;
        Asset::AssetManager* m_assetMgr = nullptr;

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

        // bone
        BufferHandle m_dummyBoneUBO;
    };

} // namespace ChikaEngine::Render