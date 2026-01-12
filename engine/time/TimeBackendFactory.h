#pragma once

#include "GlfwTimeBackend.h"
#include "ITimeBackend.h"
#include "TimeDesc.h"

#include <memory>
#include <stdexcept>
namespace ChikaEngine::Time
{

    static std::unique_ptr<ITimeBackend> TimeBackendFactory(TimeDesc desc)
    {
        switch (desc.backend)
        {
        case TimeBackendTypes::None:
            throw std::runtime_error("None Time Backend");
        case TimeBackendTypes::GLFW:
            return std::make_unique<GlfwTimeBackend>();
        default:
            throw std::runtime_error("None Time Backend");
        }
        return nullptr;
    }
} // namespace ChikaEngine::Time