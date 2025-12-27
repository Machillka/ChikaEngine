#pragma once
namespace ChikaEngine
{
    class Engine
    {
      public:
        bool tick_one_frame(float delta_time);
        void initialize();
        void shutdown();
        void run();
    };
} // namespace ChikaEngine
