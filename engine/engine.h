#pragma once
namespace ChikaEngine
{
    class Engine
    {
      public:
        Engine();
        ~Engine();
        void Initialize();
        bool Tick(float delta_time);
        void Render();
        void Shutdown();
    };
} // namespace ChikaEngine
