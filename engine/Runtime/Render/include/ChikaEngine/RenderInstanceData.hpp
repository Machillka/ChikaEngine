#pragma once

#include "ChikaEngine/RenderWorld.hpp"

#include <cstdint>
#include <type_traits>

namespace ChikaEngine::Render
{
    enum class RenderInstanceClass : uint8_t
    {
        StaticOpaque,
        Skinned,
        Transparent,
        InvalidResource,
    };

    /** @brief Compact frame-local object data consumed by visibility and packet preparation. */
    struct alignas(16) RenderInstanceData
    {
        Math::Mat4 transform = Math::Mat4::Identity();
        RenderBounds bounds;
        Resource::MeshHandle mesh;
        Resource::MaterialHandle material;
        RenderObjectHandle handle;
        uint32_t layerMask = 0xFFFFFFFFu;
        uint32_t sourceIndex = 0;
        RenderObjectFlags flags = RenderObjectFlags::None;
        RenderInstanceClass instanceClass = RenderInstanceClass::InvalidResource;
        uint8_t padding[3]{};
    };

    static_assert(std::is_trivially_copyable_v<RenderInstanceData>);
    static_assert(alignof(RenderInstanceData) == 16);
    static_assert(sizeof(RenderInstanceData) % 16 == 0);
} // namespace ChikaEngine::Render
