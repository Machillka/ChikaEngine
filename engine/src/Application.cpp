#include "ChikaEngine/Application.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/profiler/Profiler.hpp"
#include <chrono>
#include <exception>
#include <vector>

namespace ChikaEngine::Engine
{
    EngineContextCreateInfo Application::CreateEngineContextInfo() const
    {
        return {};
    }

    int Application::Run()
    {
        Profiler::ProfilerSession& profiler = Profiler::ProfilerSession::Get();
#if defined(CHIKA_DEBUG)
        profiler.Initialize({ .enabled = true });
#else
        profiler.Initialize({ .enabled = false });
#endif
        profiler.SetCurrentThreadName("Main Thread");

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
            {
                profiler.Shutdown();
                return 1;
            }

            m_applicationInitialized = true;
            OnInitialize();

            uint64_t frameIndex = 0;
            while (!m_exitRequested && !m_context.ShouldClose())
            {
                CHIKA_PROFILE_FRAME(frameIndex);
                float deltaTime = 0.0f;
                {
                    CHIKA_PROFILE_SCOPE("Application.BeginFrame");
                    deltaTime = m_context.BeginFrame();
                }
                {
                    CHIKA_PROFILE_SCOPE("Application.Update");
                    OnUpdate(deltaTime);
                }

                // Measure the shared engine tick so every application observes the same CPU boundary.
                const auto tickBegin = std::chrono::steady_clock::now();
                {
                    CHIKA_PROFILE_SCOPE("Application.EngineTick");
                    m_context.Tick(deltaTime);
                }
                const auto tickEnd = std::chrono::steady_clock::now();
                const double tickTimeMs = std::chrono::duration<double, std::milli>(tickEnd - tickBegin).count();

                std::vector<Profiler::ProfilerGpuTiming> gpuTimings;
                if (const auto* renderer = m_context.GetRenderer(); renderer && renderer->GetRHIHandle())
                {
                    const auto& sourceTimings = renderer->GetRHIHandle()->GetPassGpuTimings();
                    gpuTimings.reserve(sourceTimings.size());
                    for (const Render::RenderPassGpuTiming& timing : sourceTimings)
                    {
                        gpuTimings.push_back({
                            .frameId = timing.frameIndex,
                            .name = timing.name,
                            .queueId = timing.queueId,
                            .durationNs = static_cast<uint64_t>(timing.gpuTimeMs * 1'000'000.0),
                            .valid = timing.valid,
                        });
                    }
                }
                profiler.SubmitGpuTimings(gpuTimings);

                {
                    CHIKA_PROFILE_SCOPE("Application.FrameComplete");
                    OnFrameComplete({ .deltaTime = deltaTime, .engineTickCpuTimeMs = tickTimeMs, .frameIndex = frameIndex });
                }
                ++frameIndex;
            }

            shutdownApplication();
            m_context.Shutdown();
            profiler.Shutdown();
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
        profiler.Shutdown();
        return 1;
    }
} // namespace ChikaEngine::Engine
