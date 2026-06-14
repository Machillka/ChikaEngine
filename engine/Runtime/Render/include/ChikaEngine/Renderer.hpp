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

#include "Camera.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/ResourceHandle.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RenderDiagnostics.hpp"
#include "ChikaEngine/RenderWorld.hpp"
#include "ChikaEngine/RHIResourceHandle.hpp"
#include "RenderGraph.hpp"
#include "ChikaEngine/rhi/RHIBackendFactory.hpp"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include "ChikaEngine/math/mat4.h"

namespace ChikaEngine::Render
{
    enum class RenderPipelineMode
    {
        Forward,
        Deferred,
    };

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
        int renderMode;
        int _padding;
    };

    struct RendererCreateInfo
    {
        void* windowHandle = nullptr;
        Asset::AssetManager* assetManager = nullptr;
        uint32_t width = 1920;
        uint32_t height = 1080;
        RHIBackendTypes backendType = RHIBackendTypes::Default;
        RenderPipelineMode pipelineMode = RenderPipelineMode::Forward;
    };

    class Renderer
    {
      public:
        ~Renderer();

        // TODO[x]: asset manager 应当是依赖注入
        void Initialize(const RendererCreateInfo& createInfo);
        void Shutdown();
        void BeginFrame();
        void Tick(float deltaTimt); // 渲染逻辑 Tick
        void EndFrame();

      public:
        /**
         * @brief 提交帧边界生成的不可变 RenderWorld Snapshot。
         */
        void SubmitRenderWorldSnapshot(std::shared_ptr<const RenderWorldSnapshot> snapshot);

        /**
         * @brief 将编辑器相机转换为 Render View 数据，Gameplay Bridge 不持有 Camera 指针。
         */
        RenderView CreateEditorView();

      public:
        // 相机设置
        Camera* GetActiveCamera() const
        {
            return m_activeCamera;
        }
        float GetViewportAspectRatio() const
        {
            return static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight);
        }

      public:
        Asset::AssetManager* GetAssetManager();
        Resource::ResourceManager* GetResourceManager();

      public:
        IRHIDevice* GetRHIHandle() const;
        void SetPipelineMode(RenderPipelineMode mode)
        {
            m_pipelineMode = mode;
        }
        RenderPipelineMode GetPipelineMode() const
        {
            return m_pipelineMode;
        }

        /**
         * @brief 返回最近完成 RenderGraph 执行阶段的帧统计。
         *
         * 该值供 Editor、自动测试和性能基线读取，不参与渲染控制逻辑。
         */
        const RenderFrameStatistics& GetFrameStatistics() const
        {
            return m_frameStatistics;
        }

      public:
        void SubmitImGuiData(void* drawData)
        {
            m_imguiDrawData = drawData;
        }
        // void RequestResize(uint32_t width, uint32_t height);
        uint32_t GetViewportWidth() const
        {
            return m_viewportWidth;
        }
        uint32_t GetViewportHeight() const
        {
            return m_viewportHeight;
        }

        TextureHandle GetOffscreenTexture() const
        {
            return m_offscreenColor;
        }

      public:
        // handle window resize -> notified by window layer
        void OnWindowResize(uint32_t width, uint32_t height);
        // Handle view port resize -> notified by editor layer
        void OnViewResize(uint32_t width, uint32_t height);

      private:
        void BuildRenderGraph();
        void UpdateSceneDataFromSnapshot();
        void PrepareSnapshotResources();

      private:
        void AddMainScenePass();
        void AddGBufferPass();
        void AddDeferredLightingPass();
        void AddUploadPasses();
        void AddShadowPass();
        void HandlePendingResize();
        void AddImGuiPass();
        void DrawSceneGeometry(IRHICommandList* cmd, bool shadowPass, bool gbufferPass);
        void CreateDeferredResources();

      private:
        std::shared_ptr<const RenderWorldSnapshot> m_snapshot;
        std::unordered_map<uint64_t, BufferHandle> m_boneBuffers;

      private:
        // 视口尺寸状态
        uint32_t m_viewportWidth = 1920;
        uint32_t m_viewportHeight = 1080;

        // bool m_isResizePending = false;
        // uint32_t m_pendingWidth = 1920;
        // uint32_t m_pendingHeight = 1080;

        // 给 view 试图重建构建的记录参数
        bool m_isViewResizePending = false;
        uint32_t m_pendingViewWidth;
        uint32_t m_pendingViewHeight;

        void* m_imguiDrawData = nullptr;

      private:
        void* m_window = nullptr;
        std::unique_ptr<IRHIDevice> m_rhi = nullptr;
        std::unique_ptr<RenderGraph> m_renderGraph = nullptr;
        RenderPipelineMode m_pipelineMode = RenderPipelineMode::Forward;

        // window 大小
        uint32_t m_width = 1920;
        uint32_t m_height = 1080;
        TextureHandle m_offscreenColor;
        RGTextureHandle m_rgOffscreen;

        std::unique_ptr<Resource::ResourceManager> m_resourceMgr = nullptr;
        Asset::AssetManager* m_assetMgr = nullptr;

        TextureHandle m_depthTexture;
        RGTextureHandle m_rgSwapchain;
        RGTextureHandle m_rgDepth;
        RGTextureHandle m_rgGBufferAlbedo;
        RGTextureHandle m_rgGBufferNormal;
        RGTextureHandle m_rgGBufferMaterial;

        TextureHandle m_gbufferAlbedoTexture;
        TextureHandle m_gbufferNormalTexture;
        TextureHandle m_gbufferMaterialTexture;
        PipelineHandle m_deferredLightingPipeline;
        Shader::ShaderProgramInterface m_deferredLightingInterface;
        ResourceBindingHandle m_deferredSceneBinding;
        ResourceBindingHandle m_deferredAlbedoBinding;
        ResourceBindingHandle m_deferredNormalBinding;
        ResourceBindingHandle m_deferredMaterialBinding;

        // shadow
        TextureHandle m_shadowDepthTexture;
        TextureHandle m_shadowColorTexture; // TODO: add pass 提供不依赖 color attachment 的接口
        RGTextureHandle m_rgShadowDepth;
        RGTextureHandle m_rgShadowColor;

        float m_time = 0.0f;
        RenderFrameStatistics m_frameStatistics;

        BufferHandle m_sceneUBO;
        TextureHandle m_dummyTexture;

        // bone
        BufferHandle m_dummyBoneUBO;

      private:
        // 指向当前 被激活的相机
        Camera* m_activeCamera = nullptr;

        // 默认指针
        std::unique_ptr<Camera> m_defaultCamera;
    };

} // namespace ChikaEngine::Render
