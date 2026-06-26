#pragma once

#include "ChikaEngine/GpuDrivenData.hpp"
#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/RHICapabilities.hpp"

#include <cstdint>

namespace ChikaEngine::Render
{
    struct GpuDrivenSubmissionDesc
    {
        BufferHandle indirectArguments;
        uint64_t offset = 0;
        uint32_t commandCount = 0;
        uint32_t stride = sizeof(GpuIndexedIndirectCommand);
    };

    /**
     * @brief Records indexed indirect submission through RHI after validating the Phase 4 contract.
     *
     * This helper owns policy checks only; buffer lifetime, barriers and binding state remain RenderGraph
     * and pass responsibilities.
     */
    bool RecordGpuDrivenIndexedIndirect(IRHICommandList& commandList, const RHICapabilities& capabilities, const GpuDrivenSubmissionDesc& desc);
} // namespace ChikaEngine::Render
