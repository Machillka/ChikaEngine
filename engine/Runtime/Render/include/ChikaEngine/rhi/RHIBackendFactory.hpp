/*!
 * @file RHIBackendFactory.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief Creates RHI backend devices.
 * @version 0.1
 * @date 2026-03-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "ChikaEngine/IRHIDevice.hpp"
#include <memory>

namespace ChikaEngine::Render
{
    enum class RHIBackendTypes
    {
        Auto,
        Vulkan,
        OpenGL,
        Default,
    };

    class RHIBackendFactory
    {
      public:
        static std::unique_ptr<IRHIDevice> CreateRHIDevice(RHIBackendTypes type, const RHI_InitParams& params);
    };

} // namespace ChikaEngine::Render
