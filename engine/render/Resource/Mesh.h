#pragma once

#include "render/rhi/RHIResources.h"
#include "render/rhi/RHITypes.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ChikaEngine::Render
{
    struct Vertex
    {
        std::array<float, 3> position;
        std::array<float, 3> normal;
        std::array<float, 2> uv;
    };

    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
    };

    // 对 RHI 的封装
    struct RHIMesh
    {
        const IRHIVertexArray* vao = nullptr;
        uint32_t indexCount = 0;
        IndexType indexType = IndexType::Uint32;
    };

    using MeshHandle = std::uint32_t;
} // namespace ChikaEngine::Render