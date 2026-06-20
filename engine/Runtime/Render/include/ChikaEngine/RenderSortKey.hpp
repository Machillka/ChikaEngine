#pragma once

#include "ChikaEngine/RenderQueue.hpp"

#include <compare>
#include <cstdint>

namespace ChikaEngine::Render
{
    /** @brief Explicit non-packed key avoids bit-width overflow while preserving state grouping. */
    struct RenderOpaqueSortKey
    {
        uint8_t pass = 0;
        uint32_t pipeline = 0;
        uint32_t material = 0;
        uint32_t mesh = 0;
        uint32_t object = 0;
        auto operator<=>(const RenderOpaqueSortKey&) const = default;
    };

    /** @brief Builds the deterministic opaque ordering key used by serial and job paths. */
    RenderOpaqueSortKey BuildOpaqueSortKey(const RenderPacket& packet);
} // namespace ChikaEngine::Render
