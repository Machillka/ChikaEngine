#pragma once

#include "ITimeBackend.h"
namespace ChikaEngine::Time
{

    class GlfwTimeBackend : public ITimeBackend
    {
        double GetCurrentTimeSeconds() const override;
    };
} // namespace ChikaEngine::Time