#include "ChikaEngine/Application.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include <chrono>
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

            uint64_t frameIndex = 0;
            while (!m_exitRequested && !m_context.ShouldClose())
            {
                const float deltaTime = m_context.BeginFrame();
                OnUpdate(deltaTime);

                // Measure the shared engine tick so every application observes the same CPU boundary.
                const auto tickBegin = std::chrono::steady_clock::now();
                m_context.Tick(deltaTime);
                const auto tickEnd = std::chrono::steady_clock::now();
                const double tickTimeMs = std::chrono::duration<double, std::milli>(tickEnd - tickBegin).count();
                OnFrameComplete({ .deltaTime = deltaTime, .engineTickCpuTimeMs = tickTimeMs, .frameIndex = frameIndex++ });
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
