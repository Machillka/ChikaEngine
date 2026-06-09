#pragma once

#include "ChikaEngine/EngineContext.hpp"

namespace ChikaEngine::Engine
{
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
