#pragma once

#include "ChikaEngine/EngineContext.hpp"

#include <cstdint>

namespace ChikaEngine::Engine
{
    /**
     * @brief Describes one completed engine frame at the application boundary.
     *
     * Keeping the observation at this boundary lets tools measure the whole engine tick
     * without adding benchmark-only dependencies to EngineContext or Renderer.
     */
    struct ApplicationFrameMetrics
    {
        float deltaTime = 0.0f;
        double engineTickCpuTimeMs = 0.0;
        uint64_t frameIndex = 0;
    };

    class Application
    {
      public:
        virtual ~Application() = default;

        int Run();
        void RequestExit()
        {
            m_exitRequested = true;
        }

      protected:
        virtual EngineContextCreateInfo CreateEngineContextInfo() const;
        virtual void OnInitialize() {}
        virtual void OnUpdate(float deltaTime) {}
        /**
         * @brief Receives a stable observation after EngineContext has completed the frame.
         *
         * The default no-op preserves existing applications while benchmark and profiling
         * frontends can collect metrics without duplicating the main loop.
         */
        virtual void OnFrameComplete(const ApplicationFrameMetrics&) {}
        virtual void OnShutdown() {}

        EngineContext& GetEngineContext()
        {
            return m_context;
        }

        const EngineContext& GetEngineContext() const
        {
            return m_context;
        }

      private:
        EngineContext m_context;
        bool m_exitRequested = false;
        bool m_applicationInitialized = false;
    };
} // namespace ChikaEngine::Engine
