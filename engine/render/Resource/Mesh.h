#pragma once

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

    using MeshHandle = std::uint32_t;
} // namespace ChikaEngine::Render