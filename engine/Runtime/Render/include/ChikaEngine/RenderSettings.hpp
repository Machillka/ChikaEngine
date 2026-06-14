#pragma once

#include <cstdint>

namespace ChikaEngine::Render
{
    enum class RenderPipelineMode
    {
        Forward,
        Deferred,
    };

    /**
     * @brief 保存 Renderer 可配置策略，避免 Pass 业务常量散落在 Facade。
     */
    struct RenderSettings
    {
        RenderPipelineMode pipelineMode = RenderPipelineMode::Forward;
        uint32_t shadowResolution = 2048;
    };
} // namespace ChikaEngine::Render
