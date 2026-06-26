#pragma once

#include <cstdint>

namespace ChikaEngine::Render
{
    enum class RenderCpuMode
    {
        Serial,
        Jobs,
    };

    enum class RenderPathMode : uint8_t
    {
        SerialCpu,
        JobCpu,
        GpuDriven,
    };

    enum class RenderPathFallbackReason : uint8_t
    {
        None,
        MissingCompute,
        MissingStorageBuffer,
        MissingIndirectDraw,
        MissingGpuDrivenConsumer,
        JobsUnavailable,
        StrictGpuUnavailable,
    };

    enum class RenderPipelineMode
    {
        Forward,
        Deferred,
    };

    /**
     * @brief 保存线性 HDR 到显示输出阶段的可配置策略。
     */
    struct PostProcessSettings
    {
        float exposure = 1.0f;
        float bloomThreshold = 1.0f;
        float bloomIntensity = 0.08f;
        bool bloomEnabled = true;
        bool fxaaEnabled = true;
        bool toneMappingEnabled = true;
    };

    /**
     * @brief 保存当前单方向光阴影路径的质量参数。
     *
     * CSM/Atlas 后续会扩展该结构，但调用方不直接依赖 Shadow Texture 实现。
     */
    struct ShadowSettings
    {
        uint32_t resolution = 2048;
        float depthBias = 0.001f;
        float normalBias = 0.005f;
        uint32_t pcfRadius = 1;
    };

    /**
     * @brief 保存 Renderer 可配置策略，避免 Pass 业务常量散落在 Facade。
     */
    struct RenderSettings
    {
        RenderPipelineMode pipelineMode = RenderPipelineMode::Forward;
        RenderPathMode requestedPath = RenderPathMode::JobCpu;
        RenderCpuMode cpuMode = RenderCpuMode::Jobs;
        bool strictGpuDriven = false;
        uint32_t parallelObjectThreshold = 2'048;
        uint32_t visibilityGrainSize = 256;
        uint32_t packetGrainSize = 256;
        uint32_t parallelSortThreshold = 4'096;
        uint32_t sortGrainSize = 1'024;
        float ambientIntensity = 0.12f;
        PostProcessSettings postProcess;
        ShadowSettings shadows;
        bool debugDrawAABBs = false;
        bool debugDrawFrustums = false;
    };
} // namespace ChikaEngine::Render
