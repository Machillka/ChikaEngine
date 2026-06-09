#include "ChikaEngine/Application.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include <exception>

namespace ChikaEngine::Engine
{
    EngineContextCreateInfo Application::CreateEngineContextInfo() const
    {
        return {};
    }

    int Application::Run()
    {
        const auto shutdownApplication = [this]()
        {
            if (!m_applicationInitialized)
                return;

            m_applicationInitialized = false;
            OnShutdown();
        };

        try
        {
            if (!m_context.Initialize(CreateEngineContextInfo()))
                return 1;

            m_applicationInitialized = true;
            OnInitialize();

            while (!m_exitRequested && !m_context.ShouldClose())
            {
                const float deltaTime = m_context.BeginFrame();
                OnUpdate(deltaTime);
                m_context.Tick(deltaTime);
            }

            shutdownApplication();
            m_context.Shutdown();
            return 0;
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("Application", "Unhandled exception: {}", exception.what());
        }
        catch (...)
        {
            LOG_ERROR("Application", "Unhandled unknown exception");
        }

        try
        {
            shutdownApplication();
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("Application", "Exception during application shutdown: {}", exception.what());
        }
        catch (...)
        {
            LOG_ERROR("Application", "Unknown exception during application shutdown");
        }

        m_context.Shutdown();
        return 1;
    }
} // namespace ChikaEngine::Engine
