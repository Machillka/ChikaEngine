#pragma once

#include "ChikaEngine/GpuDrivenData.hpp"
#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/RenderGraphBlackboard.hpp"

namespace ChikaEngine::Render
{
    struct GpuDrivenGraphDesc
    {
        uint32_t instanceCapacity = 0;
        uint32_t drawGroupCapacity = 0;
        uint32_t visibleCapacity = 0;
        bool addGraphicsConsumer = true;
    };

    struct GpuDrivenGraphHandles
    {
        RGBufferHandle instanceData;
        RGBufferHandle drawGroups;
        RGBufferHandle visibilityFlags;
        RGBufferHandle visibleInstances;
        RGBufferHandle indirectArgs;
        RGBufferHandle indirectReset;
    };

    /** @brief Adds the minimal upload -> compute cull -> graphics-consumer buffer chain to a RenderGraph. */
    GpuDrivenGraphHandles AddGpuDrivenBufferFlow(RenderGraph& graph, RenderGraphBlackboard& blackboard, const GpuDrivenGraphDesc& desc);

    namespace RenderGraphSemantic
    {
        inline constexpr std::string_view GpuInstanceData = "GpuDriven.InstanceData";
        inline constexpr std::string_view GpuDrawGroups = "GpuDriven.DrawGroups";
        inline constexpr std::string_view GpuVisibilityFlags = "GpuDriven.VisibilityFlags";
        inline constexpr std::string_view GpuVisibleInstances = "GpuDriven.VisibleInstances";
        inline constexpr std::string_view GpuIndirectArgs = "GpuDriven.IndirectArgs";
        inline constexpr std::string_view GpuIndirectReset = "GpuDriven.IndirectReset";
    } // namespace RenderGraphSemantic
} // namespace ChikaEngine::Render
