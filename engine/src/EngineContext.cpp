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
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/scene/scene.hpp"
#include <exception>
#include <memory>

namespace ChikaEngine::Engine
{
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
                m_scriptsInitialized = Scripts::ScriptsSystem::Instance().Init();
                if (!m_scriptsInitialized)
                {
                    LOG_ERROR("EngineContext", "Failed to initialize scripting");
                    Shutdown();
                    return false;
                }
            }

            m_assetManager = std::make_unique<Asset::AssetManager>();
            m_renderer = std::make_unique<Render::Renderer>();

            Render::RendererCreateInfo rendererInfo{
                .windowHandle = m_window->GetNativeHandle(),
                .assetManager = m_assetManager.get(),
                .width = m_window->GetWidth(),
                .height = m_window->GetHeight(),
                .backendType = createInfo.rendererBackend,
                .pipelineMode = createInfo.renderPipeline,
            };
            m_renderer->Initialize(rendererInfo);
            m_window->SetResizeCallback(
                [this](uint32_t width, uint32_t height)
                {
                    if (m_renderer)
                        m_renderer->OnWindowResize(width, height);
                });

            m_scene = std::make_unique<Framework::Scene>();
            m_scene->Initialize({
                .renderInstance = m_renderer.get(),
                .fixedDeltaTime = createInfo.fixedDeltaTime,
                .maxPhysicsStepsPerFrame = createInfo.maxPhysicsStepsPerFrame,
            });

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
        if (m_scene)
        {
            m_scene->Shutdown();
            m_scene.reset();
        }

        if (m_renderer)
        {
            m_renderer->Shutdown();
            m_renderer.reset();
        }

        m_assetManager.reset();

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
        if (!m_initialized)
            return 0.0f;

        m_window->Tick();

        if (m_timeInitialized)
            Time::TimeSystem::Update();
        if (m_inputInitialized)
            Input::InputSystem::Update();

        return m_timeInitialized ? Time::TimeSystem::GetDeltaTime() : 0.0f;
    }

    void EngineContext::Tick(float deltaTime)
    {
        if (!m_initialized)
            return;

        m_renderer->BeginFrame();
        m_scene->Tick(deltaTime);
        m_renderer->Tick(deltaTime);
        m_renderer->EndFrame();
    }

    bool EngineContext::ShouldClose() const
    {
        return !m_window || m_window->ShouldClose();
    }
} // namespace ChikaEngine::Engine
