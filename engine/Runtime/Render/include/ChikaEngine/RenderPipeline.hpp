#pragma once

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RenderDiagnostics.hpp"
#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/RenderSettings.hpp"
#include "ChikaEngine/RenderWorld.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace ChikaEngine::Render
{
    struct SceneData
    {
        Math::Mat4 cameraVP;
        Math::Mat4 lightVP;
        float lightDir[4];
        float viewPos[4];
    };

    struct PC
    {
        Math::Mat4 model;
        int isShadowPass;
        int isSkinned;
        int renderMode;
        int _padding;
    };

    struct RenderPipelineCreateInfo
    {
        IRHIDevice* rhi = nullptr;
        Asset::AssetManager* assetManager = nullptr;
        Resource::ResourceManager* resourceManager = nullptr;
        RenderSettings* settings = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    /**
     * @brief 组织 RenderGraph Pass 并消费不可变 RenderWorld Snapshot。
     *
     * 该类不读取 Gameplay；输入只包含 Render Proxy、View、Light 和 Render Resource。
     */
    class RenderPipeline
    {
      public:
        ~RenderPipeline();
        /** @brief 创建 Pipeline 自有资源并连接 RenderGraph、Resource 与 RHI 边界。 */
        void Initialize(const RenderPipelineCreateInfo& createInfo);
        /** @brief 等待 GPU 后显式释放 Pipeline 自有资源。 */
        void Shutdown();
        void BeginFrame();
        /** @brief 从当前不可变 Snapshot 构建并执行本帧 RenderGraph。 */
        void Execute(float deltaTime);
        /** @brief 原子替换下一帧消费的只读世界快照。 */
        void SubmitSnapshot(std::shared_ptr<const RenderWorldSnapshot> snapshot);
        void SubmitImGuiData(void* drawData);
        void OnWindowResize(uint32_t width, uint32_t height);
        void OnViewResize(uint32_t width, uint32_t height);

        uint32_t GetViewportWidth() const;
        uint32_t GetViewportHeight() const;
        TextureHandle GetOffscreenTexture() const;
        const RenderFrameStatistics& GetFrameStatistics() const;

      private:
        void BuildRenderGraph();
        void AddMainScenePass();
        void AddGBufferPass();
        void AddDeferredLightingPass();
        void AddUploadPasses();
        void AddShadowPass();
        void AddImGuiPass();
        void DrawSceneGeometry(IRHICommandList* cmd, bool shadowPass, bool gbufferPass);
        void CreateDeferredResources();
        void HandlePendingResize();
        void UpdateSceneDataFromSnapshot();
        void PrepareSnapshotResources();
        /** @brief 释放由 Pipeline 创建、因此必须由 Pipeline 回收的 RHI 资源。 */
        void DestroyOwnedResources();

        IRHIDevice* m_rhi = nullptr;
        Asset::AssetManager* m_assetMgr = nullptr;
        Resource::ResourceManager* m_resourceMgr = nullptr;
        RenderSettings* m_settings = nullptr;
        std::unique_ptr<RenderGraph> m_renderGraph;
        std::shared_ptr<const RenderWorldSnapshot> m_snapshot;
        std::unordered_map<uint64_t, BufferHandle> m_boneBuffers;

        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_viewportWidth = 0;
        uint32_t m_viewportHeight = 0;
        bool m_isViewResizePending = false;
        bool m_dummyTextureTransitioned = false;
        uint32_t m_pendingViewWidth = 0;
        uint32_t m_pendingViewHeight = 0;
        void* m_imguiDrawData = nullptr;

        TextureHandle m_offscreenColor;
        TextureHandle m_depthTexture;
        TextureHandle m_dummyTexture;
        TextureHandle m_shadowDepthTexture;
        TextureHandle m_shadowColorTexture;
        BufferHandle m_sceneUBO;
        BufferHandle m_dummyBoneUBO;
        ShaderHandle m_deferredLightingVertexShader;
        ShaderHandle m_deferredLightingFragmentShader;
        PipelineHandle m_deferredLightingPipeline;

        RGTextureHandle m_rgOffscreen;
        RGTextureHandle m_rgSwapchain;
        RGTextureHandle m_rgDepth;
        RGTextureHandle m_rgGBufferAlbedo;
        RGTextureHandle m_rgGBufferNormal;
        RGTextureHandle m_rgGBufferMaterial;
        RGTextureHandle m_rgShadowDepth;
        RGTextureHandle m_rgShadowColor;

        Shader::ShaderProgramInterface m_deferredLightingInterface;
        ResourceBindingHandle m_deferredSceneBinding;
        ResourceBindingHandle m_deferredAlbedoBinding;
        ResourceBindingHandle m_deferredNormalBinding;
        ResourceBindingHandle m_deferredMaterialBinding;
        RenderFrameStatistics m_frameStatistics;
        float m_time = 0.0f;
    };
} // namespace ChikaEngine::Render
