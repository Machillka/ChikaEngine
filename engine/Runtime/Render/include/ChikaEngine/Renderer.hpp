#pragma once

#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/Camera.hpp"
#include "ChikaEngine/RenderDeviceContext.hpp"
#include "ChikaEngine/RenderPath.hpp"
#include "ChikaEngine/RenderPipeline.hpp"
#include "ChikaEngine/RenderResourceSystem.hpp"
#include "ChikaEngine/RenderSettings.hpp"
#include "ChikaEngine/RenderWorld.hpp"
#include <cstdint>
#include <memory>

namespace ChikaEngine::Jobs
{
    class JobSystem;
}

namespace ChikaEngine::Render
{
    struct RendererCreateInfo
    {
        void* windowHandle = nullptr;
        Asset::AssetManager* assetManager = nullptr;
        Jobs::JobSystem* jobSystem = nullptr;
        uint32_t width = 1920;
        uint32_t height = 1080;
        RHIBackendTypes backendType = RHIBackendTypes::Default;
        RenderPipelineMode pipelineMode = RenderPipelineMode::Forward;
        RenderPathMode pathMode = RenderPathMode::JobCpu;
        RenderCpuMode cpuMode = RenderCpuMode::Jobs;
        bool strictGpuDriven = false;
        bool vSync = true;
#ifdef CHIKA_DEBUG
        bool enableValidation = true;
#else
        bool enableValidation = false;
#endif
    };

    /**
     * @brief Renderer 高层 Facade。
     *
     * Facade 只协调设备、资源、Pipeline 和编辑器 View 生命周期；Pass 资源与绘制逻辑由
     * RenderPipeline 持有，Gameplay 通过不可变 RenderWorldSnapshot 提交渲染输入。
     */
    class Renderer
    {
      public:
        ~Renderer();

        /** @brief 按 Facade 规定的依赖顺序初始化设备、资源系统与 Pipeline。 */
        void Initialize(const RendererCreateInfo& createInfo);
        /** @brief 按创建顺序的逆序关闭子系统，保证 RHI 最后销毁。 */
        void Shutdown();
        void BeginFrame();
        void Tick(float deltaTime);
        void EndFrame();
        /** @brief Returns false when the RHI could not acquire a render target for this frame. */
        bool IsFrameActive() const
        {
            return m_frameActive;
        }

        /** @brief 把不可变场景帧输入转交给 RenderPipeline，Renderer 不读取 Gameplay。 */
        void SubmitRenderWorldSnapshot(std::shared_ptr<const RenderWorldSnapshot> snapshot);
        /** @brief 将编辑器 Camera 转换为不携带对象指针的 RenderView 值。 */
        RenderView CreateEditorView();
        /** @brief 安装后端无关的最终 Overlay 录制扩展，具体 UI 实现由调用方持有。 */
        void SetOverlayPassCallback(RenderOverlayCallback callback);

        Camera* GetActiveCamera()
        {
            return &m_editorCamera;
        }
        float GetViewportAspectRatio() const;

        Asset::AssetManager* GetAssetManager() const
        {
            return m_assetManager;
        }
        Resource::ResourceManager* GetResourceManager() const
        {
            return m_resourceSystem.GetResourceManager();
        }
        IRHIDevice* GetRHIHandle() const
        {
            return m_deviceContext.GetRHI();
        }

        void SetPipelineMode(RenderPipelineMode mode)
        {
            m_settings.pipelineMode = mode;
        }
        RenderPipelineMode GetPipelineMode() const
        {
            return m_settings.pipelineMode;
        }
        /** @brief Selects the serial oracle or the jobs-based renderer preparation path. */
        void SetCpuMode(RenderCpuMode mode)
        {
            m_settings.cpuMode = mode;
            m_settings.requestedPath = ToRenderPathMode(mode);
        }
        /** @brief Returns the active renderer CPU preparation policy. */
        RenderCpuMode GetCpuMode() const
        {
            return m_settings.cpuMode;
        }
        /** @brief Selects the requested full render path; unsupported GPU paths still report fallback. */
        void SetRenderPathMode(RenderPathMode mode)
        {
            m_settings.requestedPath = mode;
            m_settings.cpuMode = ToRenderCpuMode(mode);
        }
        /** @brief Returns the user-requested full render path. */
        RenderPathMode GetRequestedRenderPathMode() const
        {
            return m_settings.requestedPath;
        }
        /** @brief 返回当前渲染质量配置的只读快照，供 Editor 展示。 */
        const RenderSettings& GetSettings() const
        {
            return m_settings;
        }
        /** @brief 更新线性 HDR 曝光值；实际 UBO 在下一帧统一写入。 */
        void SetExposure(float exposure)
        {
            m_settings.postProcess.exposure = exposure;
        }
        /** @brief 开关轻量 Bloom Composite。 */
        void SetBloomEnabled(bool enabled)
        {
            m_settings.postProcess.bloomEnabled = enabled;
        }
        /** @brief 开关输出阶段 FXAA。 */
        void SetFXAAEnabled(bool enabled)
        {
            m_settings.postProcess.fxaaEnabled = enabled;
        }
        /** @brief 设置环境光强度，作为正式 IBL 资源接入前的统一入口。 */
        void SetAmbientIntensity(float intensity)
        {
            m_settings.ambientIntensity = intensity;
        }

        const RenderFrameStatistics& GetFrameStatistics() const
        {
            return m_pipeline.GetFrameStatistics();
        }
        /** @brief 暴露只读 RenderGraph 诊断快照给编辑器工具。 */
        const RenderGraphDebugSnapshot& GetRenderGraphDebugSnapshot() const
        {
            return m_pipeline.GetRenderGraphDebugSnapshot();
        }
        /** @brief 追加当前 RenderWorld 的编辑器调试线框，不影响正式渲染提交。 */
        void AppendDebugGizmos() const
        {
            m_pipeline.AppendDebugGizmos();
        }
        /** @brief 开关全部 Render Proxy 的 World AABB Overlay。 */
        void SetDebugDrawAABBs(bool enabled)
        {
            m_settings.debugDrawAABBs = enabled;
        }
        /** @brief 查询 World AABB Overlay 是否开启。 */
        bool IsDebugDrawAABBsEnabled() const
        {
            return m_settings.debugDrawAABBs;
        }
        /** @brief 开关 Render View 与投影阴影 Light Frustum Overlay。 */
        void SetDebugDrawFrustums(bool enabled)
        {
            m_settings.debugDrawFrustums = enabled;
        }
        /** @brief 查询 Frustum Overlay 是否开启。 */
        bool IsDebugDrawFrustumsEnabled() const
        {
            return m_settings.debugDrawFrustums;
        }
        uint32_t GetViewportWidth() const
        {
            return m_pipeline.GetViewportWidth();
        }
        uint32_t GetViewportHeight() const
        {
            return m_pipeline.GetViewportHeight();
        }
        TextureHandle GetOffscreenTexture() const
        {
            return m_pipeline.GetOffscreenTexture();
        }

        void OnWindowResize(uint32_t width, uint32_t height);
        void OnViewResize(uint32_t width, uint32_t height);

      private:
        RenderDeviceContext m_deviceContext;
        RenderResourceSystem m_resourceSystem;
        RenderPipeline m_pipeline;
        RenderSettings m_settings;
        Camera m_editorCamera;
        Asset::AssetManager* m_assetManager = nullptr;
        bool m_initialized = false;
        bool m_frameActive = false;
    };
} // namespace ChikaEngine::Render
