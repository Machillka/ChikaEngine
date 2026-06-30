#pragma once

#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/Window/WinDesc.hpp"
#include "ChikaEngine/project/RuntimeMode.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>

namespace ChikaEngine::Asset
{
    class AssetManager;
}

namespace ChikaEngine::Framework
{
    class Scene;
    class SceneManager;
} // namespace ChikaEngine::Framework

namespace ChikaEngine::Platform
{
    class IWindow;
}

namespace ChikaEngine::Jobs
{
    class JobSystem;
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
        Render::RenderPathMode renderPathMode = Render::RenderPathMode::JobCpu;
        Render::RenderCpuMode renderCpuMode = Render::RenderCpuMode::Jobs;
        bool strictGpuDriven = false;
        float fixedDeltaTime = 1.0f / 60.0f;
        uint32_t maxPhysicsStepsPerFrame = 4;
        bool enableJobs = true;
        uint32_t jobWorkerCount = 0;
        uint32_t reservedJobThreads = 2;
        Project::RuntimeMode runtimeMode = Project::RuntimeMode::Editor;
        std::filesystem::path contentRoot = "Assets";
        bool createContentRoot = true;
        bool scanAssets = true;
        bool createMissingMeta = true;
        bool importAssets = true;
        bool enableHotReload = true;
        bool createDefaultScene = true;
        bool useEditorView = true;
    };

    class EngineContext
    {
      public:
        EngineContext();
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

        Jobs::JobSystem* GetJobSystem() const
        {
            return m_jobSystem.get();
        }

        Framework::Scene* GetScene() const;

        Framework::SceneManager* GetSceneManager() const
        {
            return m_sceneManager.get();
        }

        Project::RuntimeMode GetRuntimeMode() const
        {
            return m_createInfo.runtimeMode;
        }

      private:
        EngineContextCreateInfo m_createInfo;
        std::unique_ptr<Platform::IWindow> m_window;
        std::unique_ptr<Asset::AssetManager> m_assetManager;
        std::unique_ptr<Jobs::JobSystem> m_jobSystem;
        std::unique_ptr<Render::Renderer> m_renderer;
        std::unique_ptr<Framework::SceneManager> m_sceneManager;
        bool m_initialized = false;
        bool m_inputInitialized = false;
        bool m_timeInitialized = false;
        bool m_scriptsInitialized = false;
    };
} // namespace ChikaEngine::Engine
