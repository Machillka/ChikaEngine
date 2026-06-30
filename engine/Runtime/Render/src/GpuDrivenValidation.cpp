#include "ChikaEngine/GpuDrivenValidation.hpp"

#include <algorithm>
#include <iterator>

namespace ChikaEngine::Render
{
    GpuVisibilityDiff CompareVisibilitySets(std::vector<uint32_t> cpuVisibleObjectIds, std::vector<uint32_t> gpuVisibleObjectIds)
    {
        std::ranges::sort(cpuVisibleObjectIds);
        std::ranges::sort(gpuVisibleObjectIds);
        cpuVisibleObjectIds.erase(std::ranges::unique(cpuVisibleObjectIds).begin(), cpuVisibleObjectIds.end());
        gpuVisibleObjectIds.erase(std::ranges::unique(gpuVisibleObjectIds).begin(), gpuVisibleObjectIds.end());

        GpuVisibilityDiff diff;
        std::ranges::set_difference(cpuVisibleObjectIds, gpuVisibleObjectIds, std::back_inserter(diff.missingFromGpu));
        std::ranges::set_difference(gpuVisibleObjectIds, cpuVisibleObjectIds, std::back_inserter(diff.extraOnGpu));
        diff.matches = diff.missingFromGpu.empty() && diff.extraOnGpu.empty();
        return diff;
    }

    void GpuVisibilityValidationQueue::EnqueueCpuOracle(uint64_t frameId, std::vector<uint32_t> visibleObjectIds)
    {
        m_pendingCpuOracles.push_back({
            .frameId = frameId,
            .visibleObjectIds = std::move(visibleObjectIds),
            .valid = true,
        });
    }

    GpuVisibilityDiff GpuVisibilityValidationQueue::ConsumeReadback(const GpuVisibilityReadback& readback)
    {
        if (!readback.valid)
            return { .matches = false };

        const auto found = std::ranges::find(m_pendingCpuOracles, readback.frameId, &GpuVisibilityReadback::frameId);
        if (found == m_pendingCpuOracles.end())
            return { .extraOnGpu = readback.visibleObjectIds, .matches = false };

        GpuVisibilityDiff diff = CompareVisibilitySets(found->visibleObjectIds, readback.visibleObjectIds);
        m_pendingCpuOracles.erase(m_pendingCpuOracles.begin(), std::next(found));
        return diff;
    }

    void GpuVisibilityValidationQueue::Clear()
    {
        m_pendingCpuOracles.clear();
    }

    uint32_t GpuVisibilityValidationQueue::PendingCount() const
    {
        return static_cast<uint32_t>(m_pendingCpuOracles.size());
    }
} // namespace ChikaEngine::Render
