#pragma once

#include <cstdint>

namespace ChikaEngine::Render
{
    /**
     * @brief Backend-independent rendering capability snapshot reported by an RHI device.
     *
     * Renderer policy is intentionally kept outside the RHI. The device only reports what the
     * selected adapter and enabled logical-device features can execute.
     */
    struct RHICapabilities
    {
        bool compute = false;
        bool storageBuffer = false;
        bool drawIndirect = false;
        bool drawIndexedIndirect = false;
        bool multiDrawIndirect = false;
        bool drawIndirectCount = false;
        uint32_t maxDrawIndirectCount = 1;
        uint32_t maxComputeWorkGroupInvocations = 0;
    };
} // namespace ChikaEngine::Render
