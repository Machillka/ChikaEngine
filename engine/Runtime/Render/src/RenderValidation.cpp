#include "ChikaEngine/RenderValidation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>

namespace ChikaEngine::Render
{
    namespace
    {
        constexpr uint64_t kFnvOffset = 14'695'981'039'346'656'037ull;
        constexpr uint64_t kFnvPrime = 1'099'511'628'211ull;

        /** @brief Appends an integer bytewise so host padding and pointer values never affect hashes. */
        template <typename Integer> void HashInteger(uint64_t& hash, Integer value)
        {
            using Unsigned = std::make_unsigned_t<Integer>;
            const Unsigned bits = static_cast<Unsigned>(value);
            for (size_t byte = 0; byte < sizeof(Unsigned); ++byte)
            {
                hash ^= static_cast<uint8_t>(bits >> (byte * 8u));
                hash *= kFnvPrime;
            }
        }

        /** @brief Quantizes view depth to 1/1024 unit and defines finite saturation behavior. */
        int32_t QuantizeDepth(float depth)
        {
            if (!std::isfinite(depth))
                return 0;
            const double scaled = std::round(static_cast<double>(depth) * 1024.0);
            return static_cast<int32_t>(std::clamp(scaled, static_cast<double>(std::numeric_limits<int32_t>::min()), static_cast<double>(std::numeric_limits<int32_t>::max())));
        }

        auto QueueArray(const RenderQueueSet& queues)
        {
            return std::array{ &queues.shadow, &queues.forwardOpaque, &queues.gbufferOpaque, &queues.forwardTransparent };
        }
    } // namespace

    uint64_t HashVisibility(const VisibilityResult& visibility)
    {
        std::vector<uint32_t> handles;
        handles.reserve(visibility.visibleObjects.size());
        for (const RenderObjectSnapshot* object : visibility.visibleObjects)
        {
            if (object)
                handles.push_back(object->handle.raw_value);
        }
        std::ranges::sort(handles);
        uint64_t hash = kFnvOffset;
        HashInteger(hash, kRenderValidationHashVersion);
        HashInteger(hash, visibility.testedObjectCount);
        HashInteger(hash, visibility.visibleObjectCount);
        HashInteger(hash, visibility.culledObjectCount);
        for (uint32_t handle : handles)
            HashInteger(hash, handle);
        return hash;
    }

    uint64_t HashRenderPackets(const RenderQueueSet& queues)
    {
        uint64_t hash = kFnvOffset;
        HashInteger(hash, kRenderValidationHashVersion);
        for (const RenderQueue* queue : QueueArray(queues))
        {
            HashInteger(hash, static_cast<uint32_t>(queue->packets.size()));
            for (const RenderPacket& packet : queue->packets)
            {
                HashInteger(hash, static_cast<uint8_t>(packet.pass));
                HashInteger(hash, packet.pipeline.raw_value);
                HashInteger(hash, packet.material.raw_value);
                HashInteger(hash, packet.mesh.raw_value);
                HashInteger(hash, packet.object ? packet.object->handle.raw_value : UINT32_MAX);
                HashInteger(hash, QuantizeDepth(packet.viewDepth));
                HashInteger(hash, static_cast<uint8_t>(packet.instancingEligible));
            }
        }
        return hash;
    }

    uint64_t HashRenderBatches(const RenderQueueSet& queues)
    {
        uint64_t hash = kFnvOffset;
        HashInteger(hash, kRenderValidationHashVersion);
        for (const RenderQueue* queue : QueueArray(queues))
        {
            HashInteger(hash, static_cast<uint32_t>(queue->batches.size()));
            for (const RenderBatch& batch : queue->batches)
            {
                HashInteger(hash, static_cast<uint8_t>(batch.pass));
                HashInteger(hash, batch.pipeline.raw_value);
                HashInteger(hash, batch.material.raw_value);
                HashInteger(hash, batch.mesh.raw_value);
                HashInteger(hash, static_cast<uint64_t>(batch.firstPacket));
                HashInteger(hash, static_cast<uint64_t>(batch.packetCount));
                HashInteger(hash, static_cast<uint8_t>(batch.instanced));
            }
        }
        return hash;
    }

    RenderValidationHashes BuildRenderValidationHashes(const VisibilityResult& visibility, const RenderQueueSet& queues)
    {
        RenderValidationHashes result{
            .visibleSet = HashVisibility(visibility),
            .packets = HashRenderPackets(queues),
            .batches = HashRenderBatches(queues),
        };
        uint64_t drawHash = kFnvOffset;
        HashInteger(drawHash, result.packets);
        HashInteger(drawHash, result.batches);
        result.drawInput = drawHash;
        return result;
    }
} // namespace ChikaEngine::Render
