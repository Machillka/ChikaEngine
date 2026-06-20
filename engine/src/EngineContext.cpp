#include "ChikaEngine/EngineContext.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/InputDesc.h"
#include "ChikaEngine/InputSystem.h"
#include "ChikaEngine/ScriptSystem.h"
#include "ChikaEngine/TimeDesc.h"
#include "ChikaEngine/TimeSystem.h"
#include "ChikaEngine/Window/IWindow.hpp"
#include "ChikaEngine/Window/WindowFactory.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/jobs/JobSystem.hpp"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/profiler/ProfilerMacros.hpp"
#include "ChikaEngine/scene/SceneManager.hpp"
#include <exception>
#include <memory>

namespace ChikaEngine::Engine
{
    EngineContext::EngineContext() = default;

    EngineContext::~EngineContext()
    {
        Shutdown();
    }

    bool EngineContext::Initialize(const EngineContextCreateInfo& createInfo)
    {
        if (m_initialized)
            return true;

        m_createInfo = createInfo;

        try
        {
            Reflection::InitAllReflection();
            Core::UIDGenerator::Instance().Init(createInfo.machineId);

            m_window = Platform::WindowFactory::Create(createInfo.window);
            if (!m_window)
            {
                LOG_ERROR("EngineContext", "Failed to create the main window");
                Shutdown();
                return false;
            }

            if (createInfo.enableTime)
            {
                Time::TimeSystem::Init({ .backend = Time::TimeBackendTypes::GLFW });
                m_timeInitialized = true;
            }

            if (createInfo.enableInput)
            {
                Input::InputSystem::Init({ .backendType = Input::InputBackendTypes::GLFW }, m_window->GetNativeHandle());
                m_inputInitialized = true;
            }

            if (createInfo.enableScripting)
            {
                m_scriptsInitialized = Scripts::ScriptsSystem::Instance().Init(createInfo.contentRoot / "Scripts");
                if (!m_scriptsInitialized)
                {
                    LOG_ERROR("EngineContext", "Failed to initialize scripting");
                    Shutdown();
                    return false;
                }
            }

            if (createInfo.enableJobs)
            {
                m_jobSystem = std::make_unique<Jobs::JobSystem>();
                if (!m_jobSystem->Initialize({ .workerCount = createInfo.jobWorkerCount, .reservedThreads = createInfo.reservedJobThreads }))
                {
                    LOG_ERROR("EngineContext", "Failed to initialize job system");
                    Shutdown();
                    return false;
                }
            }

            m_assetManager = std::make_unique<Asset::AssetManager>();
            if (!m_assetManager->Initialize({
                    .assetRoot = createInfo.contentRoot,
                    .createRoot = createInfo.createContentRoot,
                    .scanAssets = createInfo.scanAssets,
                    .createMissingMeta = createInfo.createMissingMeta,
                    .importAssets = createInfo.importAssets,
                    .enableHotReload = createInfo.enableHotReload,
                    .jobSystem = m_jobSystem.get(),
                }))
            {
                LOG_ERROR("EngineContext", "Failed to initialize asset manager");
                Shutdown();
                return false;
            }
            m_renderer = std::make_unique<Render::Renderer>();

            Render::RendererCreateInfo rendererInfo{
                .windowHandle = m_window->GetNativeHandle(),
                .assetManager = m_assetManager.get(),
                .jobSystem = m_jobSystem.get(),
                .width = m_window->GetWidth(),
                .height = m_window->GetHeight(),
                .backendType = createInfo.rendererBackend,
                .pipelineMode = createInfo.renderPipeline,
                .cpuMode = createInfo.renderCpuMode,
                .vSync = createInfo.window.vSync,
            };
            m_renderer->Initialize(rendererInfo);
            m_window->SetResizeCallback(
                [this](uint32_t width, uint32_t height)
                {
                    if (m_renderer)
                        m_renderer->OnWindowResize(width, height);
                });

            m_sceneManager = std::make_unique<Framework::SceneManager>();
            if (!m_sceneManager->Initialize({
                    .renderInstance = m_renderer.get(),
                    .assetManager = m_assetManager.get(),
                    .fixedDeltaTime = createInfo.fixedDeltaTime,
                    .maxPhysicsStepsPerFrame = createInfo.maxPhysicsStepsPerFrame,
                    .createDefaultScene = createInfo.createDefaultScene,
                    .useEditorView = createInfo.useEditorView,
                }))
            {
                LOG_ERROR("EngineContext", "Failed to initialize scene manager");
                Shutdown();
                return false;
            }

            m_initialized = true;
            LOG_INFO("EngineContext", "Engine context initialized");
            return true;
        }
        catch (...)
        {
            Shutdown();
            throw;
        }
    }

    void EngineContext::Shutdown()
    {
        if (m_sceneManager)
        {
            m_sceneManager->Shutdown();
            m_sceneManager.reset();
        }

        if (m_renderer)
        {
            m_renderer->Shutdown();
            m_renderer.reset();
        }

        m_assetManager.reset();

        if (m_jobSystem)
        {
            m_jobSystem->Shutdown(Jobs::JobShutdownPolicy::Drain);
            m_jobSystem.reset();
        }

        if (m_scriptsInitialized)
        {
            Scripts::ScriptsSystem::Instance().Shutdown();
            m_scriptsInitialized = false;
        }

        if (m_inputInitialized)
        {
            Input::InputSystem::Shutdown();
            m_inputInitialized = false;
        }

        if (m_timeInitialized)
        {
            Time::TimeSystem::Shutdown();
            m_timeInitialized = false;
        }

        if (m_window)
        {
            m_window->Shutdown();
            m_window.reset();
        }

        m_initialized = false;
    }

    float EngineContext::BeginFrame()
    {
        CHIKA_PROFILE_SCOPE("EngineContext.BeginFrame");
        if (!m_initialized)
            return 0.0f;

        m_window->Tick();

        if (m_timeInitialized)
            Time::TimeSystem::Update();
        if (m_inputInitialized)
            Input::InputSystem::Update();
        if (m_assetManager && m_createInfo.enableHotReload)
            m_assetManager->TickHotReload();
        if (m_jobSystem)
            m_jobSystem->PumpMainThreadJobs();

        return m_timeInitialized ? Time::TimeSystem::GetDeltaTime() : 0.0f;
    }

    void EngineContext::Tick(float deltaTime)
    {
        CHIKA_PROFILE_SCOPE("EngineContext.Tick");
        if (!m_initialized)
            return;

        {
            CHIKA_PROFILE_SCOPE("Renderer.BeginFrame");
            m_renderer->BeginFrame();
        }
        {
            CHIKA_PROFILE_SCOPE("SceneManager.Tick");
            m_sceneManager->Tick(deltaTime);
        }
        {
            CHIKA_PROFILE_SCOPE("Renderer.Tick");
            m_renderer->Tick(deltaTime);
        }
        {
            CHIKA_PROFILE_SCOPE("Renderer.EndFrame");
            m_renderer->EndFrame();
        }
    }

    bool EngineContext::ShouldClose() const
    {
        return !m_window || m_window->ShouldClose();
    }

    Framework::Scene* EngineContext::GetScene() const
    {
        return m_sceneManager ? m_sceneManager->GetActiveScene() : nullptr;
    }
} // namespace ChikaEngine::Engine
