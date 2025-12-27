#pragma once
namespace chika
{
    class Engine
    {
      public:
        bool tick_one_frame(float delta_time);
        void initialize();
        void shutdown();
        void run();
    };
} // namespace chika
