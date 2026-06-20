#pragma once

#include "ChikaEngine/RenderQueue.hpp"

#include <cstdint>

namespace ChikaEngine::Render
{
    inline constexpr uint32_t kRenderValidationHashVersion = 1;

    struct RenderValidationHashes
    {
        uint32_t version = kRenderValidationHashVersion;
        uint64_t visibleSet = 0;
        uint64_t packets = 0;
        uint64_t batches = 0;
        uint64_t drawInput = 0;
        bool operator==(const RenderValidationHashes&) const = default;
    };

    /** @brief Hashes stable object identities after sorting, never pointer addresses. */
    uint64_t HashVisibility(const VisibilityResult& visibility);
    /** @brief Hashes pass/resource/object/depth packet inputs in fixed queue order. */
    uint64_t HashRenderPackets(const RenderQueueSet& queues);
    /** @brief Hashes batch keys, ranges and instancing decisions in fixed queue order. */
    uint64_t HashRenderBatches(const RenderQueueSet& queues);
    /** @brief Builds the complete versioned CPU renderer correctness oracle. */
    RenderValidationHashes BuildRenderValidationHashes(const VisibilityResult& visibility, const RenderQueueSet& queues);
} // namespace ChikaEngine::Render
