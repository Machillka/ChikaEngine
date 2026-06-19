#include "ChikaEngine/Renderer.hpp"

#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"
#include <stdexcept>

namespace ChikaEngine::Render
{
    Renderer::~Renderer()
    {
        Shutdown();
    }

    void Renderer::Initialize(const RendererCreateInfo& createInfo)
    {
        Shutdown();
        if (!createInfo.assetManager)
            throw std::invalid_argument("RendererCreateInfo::assetManager must not be null");

        m_assetManager = createInfo.assetManager;
        m_settings.pipelineMode = createInfo.pipelineMode;
        m_deviceContext.Initialize({
            .windowHandle = createInfo.windowHandle,
            .width = createInfo.width,
            .height = createInfo.height,
            .backendType = createInfo.backendType,
            .enableValidation = createInfo.enableValidation,
            .vSync = createInfo.vSync,
        });
        m_resourceSystem.Initialize(*m_deviceContext.GetRHI(), *m_assetManager);

        m_editorCamera.SetPosition(Math::Vector3(0.0f, 4.0f, 8.0f));
        m_editorCamera.SetLookAt(Math::Vector3(0.0f, 0.0f, 0.0f));
        m_editorCamera.SetPerspective(45.0f, static_cast<float>(createInfo.width) / static_cast<float>(createInfo.height), 0.1f, 1000.0f);

        m_pipeline.Initialize({
            .rhi = m_deviceContext.GetRHI(),
            .assetManager = m_assetManager,
            .resourceManager = m_resourceSystem.GetResourceManager(),
            .settings = &m_settings,
            .width = createInfo.width,
            .height = createInfo.height,
        });
        m_initialized = true;
    }

    void Renderer::Shutdown()
    {
        if (!m_initialized && !m_deviceContext.GetRHI())
            return;
        m_pipeline.Shutdown();
        m_resourceSystem.Shutdown();
        m_deviceContext.Shutdown();
        m_assetManager = nullptr;
        m_initialized = false;
        m_frameActive = false;
    }

    void Renderer::BeginFrame()
    {
        CHIKA_PROFILE_SCOPE("Renderer.BeginFrame.Internal");
        if (!m_initialized)
            return;
        m_editorCamera.SetAspectRatio(GetViewportAspectRatio());
        m_frameActive = m_deviceContext.BeginFrame();
        if (m_frameActive)
            m_pipeline.BeginFrame();
    }

    void Renderer::Tick(float deltaTime)
    {
        CHIKA_PROFILE_SCOPE("Renderer.ExecutePipeline");
        if (m_initialized && m_frameActive)
            m_pipeline.Execute(deltaTime);
    }

    void Renderer::EndFrame()
    {
        CHIKA_PROFILE_SCOPE("Renderer.Present");
        if (m_initialized && m_frameActive)
            m_deviceContext.EndFrame();
    }

    void Renderer::SubmitRenderWorldSnapshot(std::shared_ptr<const RenderWorldSnapshot> snapshot)
    {
        m_pipeline.SubmitSnapshot(std::move(snapshot));
    }

    RenderView Renderer::CreateEditorView()
    {
        return {
            .view = m_editorCamera.GetViewMatrix(),
            .projection = m_editorCamera.GetProjectionMatrix(),
            .viewProjection = m_editorCamera.GetViewProjectionMatrix(),
            .position = m_editorCamera.GetPosition(),
            .primary = true,
        };
    }

    void Renderer::SetOverlayPassCallback(RenderOverlayCallback callback)
    {
        m_pipeline.SetOverlayPassCallback(std::move(callback));
    }

    float Renderer::GetViewportAspectRatio() const
    {
        const uint32_t height = GetViewportHeight();
        return height == 0 ? 1.0f : static_cast<float>(GetViewportWidth()) / static_cast<float>(height);
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        if (!m_initialized || width == 0 || height == 0)
            return;
        m_deviceContext.Resize(width, height);
        m_pipeline.OnWindowResize(width, height);
        LOG_INFO("Renderer", "Main Window resized to {}x{}", width, height);
    }

    void Renderer::OnViewResize(uint32_t width, uint32_t height)
    {
        if (m_initialized)
            m_pipeline.OnViewResize(width, height);
    }
} // namespace ChikaEngine::Render
