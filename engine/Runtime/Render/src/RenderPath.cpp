#include "ChikaEngine/RenderPath.hpp"

namespace ChikaEngine::Render
{
    std::string_view ToString(RenderPathMode mode)
    {
        switch (mode)
        {
        case RenderPathMode::SerialCpu:
            return "serial";
        case RenderPathMode::JobCpu:
            return "jobs";
        case RenderPathMode::GpuDriven:
            return "gpu";
        }
        return "unknown";
    }

    std::string_view ToString(RenderPathFallbackReason reason)
    {
        switch (reason)
        {
        case RenderPathFallbackReason::None:
            return "none";
        case RenderPathFallbackReason::MissingCompute:
            return "missing_compute";
        case RenderPathFallbackReason::MissingStorageBuffer:
            return "missing_storage_buffer";
        case RenderPathFallbackReason::MissingIndirectDraw:
            return "missing_indirect_draw";
        case RenderPathFallbackReason::MissingGpuDrivenConsumer:
            return "missing_gpu_driven_consumer";
        case RenderPathFallbackReason::JobsUnavailable:
            return "jobs_unavailable";
        case RenderPathFallbackReason::StrictGpuUnavailable:
            return "strict_gpu_unavailable";
        }
        return "unknown";
    }

    namespace
    {
        /** @brief Chooses the best CPU fallback that can actually execute on this engine instance. */
        RenderPathMode SelectCpuFallback(bool jobSystemAvailable)
        {
            return jobSystemAvailable ? RenderPathMode::JobCpu : RenderPathMode::SerialCpu;
        }
    } // namespace

    RenderPathSelection SelectRenderPath(const RHICapabilities& capabilities, const RenderPathRequest& request)
    {
        RenderPathSelection selection{
            .requested = request.requested,
            .effective = request.requested,
            .fallback = RenderPathFallbackReason::None,
        };

        if (request.requested == RenderPathMode::SerialCpu)
            return selection;

        if (request.requested == RenderPathMode::JobCpu)
        {
            if (request.jobSystemAvailable)
                return selection;
            selection.effective = RenderPathMode::SerialCpu;
            selection.fallback = RenderPathFallbackReason::JobsUnavailable;
            return selection;
        }

        if (!capabilities.compute)
            selection.fallback = RenderPathFallbackReason::MissingCompute;
        else if (!capabilities.storageBuffer)
            selection.fallback = RenderPathFallbackReason::MissingStorageBuffer;
        else if (!capabilities.drawIndirect || !capabilities.drawIndexedIndirect)
            selection.fallback = RenderPathFallbackReason::MissingIndirectDraw;
        else if (!request.gpuDrivenConsumerAvailable)
            selection.fallback = RenderPathFallbackReason::MissingGpuDrivenConsumer;

        if (selection.fallback == RenderPathFallbackReason::None)
            return selection;

        if (request.strictGpuDriven)
        {
            selection.effective = RenderPathMode::GpuDriven;
            selection.fallback = RenderPathFallbackReason::StrictGpuUnavailable;
            selection.strictFailure = true;
            return selection;
        }

        selection.effective = SelectCpuFallback(request.jobSystemAvailable);
        return selection;
    }

    RenderPathMode ToRenderPathMode(RenderCpuMode mode)
    {
        return mode == RenderCpuMode::Serial ? RenderPathMode::SerialCpu : RenderPathMode::JobCpu;
    }

    RenderCpuMode ToRenderCpuMode(RenderPathMode mode)
    {
        return mode == RenderPathMode::SerialCpu ? RenderCpuMode::Serial : RenderCpuMode::Jobs;
    }
} // namespace ChikaEngine::Render
