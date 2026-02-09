#pragma once
#include "debug/log_macros.h"
#include "reflection/ReflectionMacros.h"
namespace ChikaEngine::Framework
{

    MCLASS(Temp)
    {
        REFLECTION_BODY(Temp)
      public:
        MFIELD()
        int health = 0;
        MFIELD()
        float attack = 1.0f;
        MFUNCTION()
        void Foo(int a)
        {
            LOG_INFO("Temp", "Foo called with a={}", a);
        }

      private:
        MFUNCTION()
        void PrintHealth()
        {
            LOG_INFO("Temp", "Health is {}", health);
        }
    };
} // namespace ChikaEngine::Framework