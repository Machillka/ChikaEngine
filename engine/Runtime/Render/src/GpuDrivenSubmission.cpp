#include "ChikaEngine/GpuDrivenSubmission.hpp"

namespace ChikaEngine::Render
{
    bool RecordGpuDrivenIndexedIndirect(IRHICommandList& commandList, const RHICapabilities& capabilities, const GpuDrivenSubmissionDesc& desc)
    {
        if (!capabilities.drawIndirect || !capabilities.drawIndexedIndirect)
            return false;
        if (!desc.indirectArguments.IsValid() || desc.commandCount == 0 || desc.stride < sizeof(GpuIndexedIndirectCommand))
            return false;
        if (desc.commandCount > 1 && !capabilities.multiDrawIndirect)
            return false;

        commandList.DrawIndexedIndirect(desc.indirectArguments, desc.offset, desc.commandCount, desc.stride);
        return true;
    }
} // namespace ChikaEngine::Render
