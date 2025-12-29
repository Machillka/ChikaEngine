#pragma once

#include <cstdint>
#include <vector>
namespace ChikaEngine::Render
{
    struct Vertex
    {
        float position[3];
        float normal[3];
        float uv[2];
    };

    struct MeshData
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
    };
    
} // namespace ChikaEngine::Render