#include "ChikaEngine/RenderSortKey.hpp"

namespace ChikaEngine::Render
{
    RenderOpaqueSortKey BuildOpaqueSortKey(const RenderPacket& packet)
    {
        return {
            .pass = static_cast<uint8_t>(packet.pass),
            .pipeline = packet.pipeline.raw_value,
            .material = packet.material.raw_value,
            .mesh = packet.mesh.raw_value,
            .object = packet.object ? packet.object->handle.raw_value : UINT32_MAX,
        };
    }
} // namespace ChikaEngine::Render
