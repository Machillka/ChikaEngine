#pragma once

#include "ChikaEngine/RHICapabilities.hpp"
#include "ChikaEngine/RenderSettings.hpp"

#include <string_view>

namespace ChikaEngine::Render
{
    struct RenderPathRequest
    {
        RenderPathMode requested = RenderPathMode::JobCpu;
        bool jobSystemAvailable = false;
        bool strictGpuDriven = false;
        bool gpuDrivenConsumerAvailable = false;
    };

    struct RenderPathSelection
    {
        RenderPathMode requested = RenderPathMode::JobCpu;
        RenderPathMode effective = RenderPathMode::JobCpu;
        RenderPathFallbackReason fallback = RenderPathFallbackReason::None;
        bool strictFailure = false;
    };

    /** @brief Converts a render path mode to a stable diagnostics and benchmark spelling. */
    std::string_view ToString(RenderPathMode mode);
    /** @brief Converts a fallback reason to a stable diagnostics spelling. */
    std::string_view ToString(RenderPathFallbackReason reason);
    /** @brief Selects an executable render path from user intent, RHI features and engine support. */
    RenderPathSelection SelectRenderPath(const RHICapabilities& capabilities, const RenderPathRequest& request);
    /** @brief Maps legacy CPU preparation modes to the new full render path enum. */
    RenderPathMode ToRenderPathMode(RenderCpuMode mode);
    /** @brief Returns the CPU preparation policy used by a selected render path. */
    RenderCpuMode ToRenderCpuMode(RenderPathMode mode);
} // namespace ChikaEngine::Render
