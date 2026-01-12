#pragma once

#include "GlfwInputBackend.h"
#include "IInputBackend.h"
#include "InputDesc.h"

#include <memory>
#include <stdexcept>
namespace ChikaEngine::Input
{
    static std::unique_ptr<IInputBackend> InputBackendFactory(InputDesc desc, void* windowHandle)
    {
        switch (desc.backendType)
        {
        case InputBackendTypes::None:
            throw std::runtime_error("None Input Backend");
        case InputBackendTypes::GLFW:
            return std::make_unique<GlfwInputBackend>(windowHandle);
        default:

            throw std::runtime_error("None Input Backend");
        }
    }
} // namespace ChikaEngine::Input