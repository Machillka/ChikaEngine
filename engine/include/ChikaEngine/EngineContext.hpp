#pragma once

#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/Window/WinDesc.hpp"
#include <cstdint>
#include <memory>

namespace ChikaEngine::Asset
{
    class AssetManager;
}

namespace ChikaEngine::Framework
{
    class Scene;
}

namespace ChikaEngine::Platform
{
    class IWindow;
}

namespace ChikaEngine::Engine
{
    struct EngineContextCreateInfo
    {
        Platform::WindowDesc window;
        uint32_t machineId = 221;
        bool enableInput = true;
        bool enableTime = true;
        bool enableScripting = true;
        Render::RHIBackendTypes rendererBackend = Render::RHIBackendTypes::Default;
        Render::RenderPipelineMode renderPipeline = Render::RenderPipelineMode::Forward;
        float fixedDeltaTime = 1.0f / 60.0f;
        uint32_t maxPhysicsStepsPerFrame = 4;
    };

    class EngineContext
    {
      public:
        EngineContext() = default;
        ~EngineContext();

        EngineContext(const EngineContext&) = delete;
        EngineContext& operator=(const EngineContext&) = delete;

        bool Initialize(const EngineContextCreateInfo& createInfo);
        void Shutdown();

        float BeginFrame();
        void Tick(float deltaTime);
        bool ShouldClose() const;

        bool IsInitialized() const
        {
            return m_initialized;
        }

        Platform::IWindow* GetWindow() const
        {
            return m_window.get();
        }

        Asset::AssetManager* GetAssetManager() const
        {
            return m_assetManager.get();
        }

        Render::Renderer* GetRenderer() const
        {
            return m_renderer.get();
        }

        Framework::Scene* GetScene() const
        {
            return m_scene.get();
        }

      private:
        EngineContextCreateInfo m_createInfo;
        std::unique_ptr<Platform::IWindow> m_window;
        std::unique_ptr<Asset::AssetManager> m_assetManager;
        std::unique_ptr<Render::Renderer> m_renderer;
        std::unique_ptr<Framework::Scene> m_scene;
        bool m_initialized = false;
        bool m_inputInitialized = false;
        bool m_timeInitialized = false;
        bool m_scriptsInitialized = false;
    };
} // namespace ChikaEngine::Engine
