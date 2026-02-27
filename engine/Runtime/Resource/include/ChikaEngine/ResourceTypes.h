#pragma once
#include <array>
#include <string>

namespace ChikaEngine::Resource
{
    using MeshHandle = uint32_t;
    using TextureHandle = uint32_t;
    using ShaderHandle = uint32_t;
    using MaterialHandle = uint32_t;
    using TextureCubeHandle = uint32_t;

    struct MeshSourceDesc
    {
        std::string path;
    };

    struct TextureSourceDesc
    {
        std::string path;
    };

    struct ShaderSourceDesc
    {
        std::string vertexPath;
        std::string fragmentPath;
    };

    struct MaterialSourceDesc
    {
        std::string path;
    };

    // Right, Left, Top, Bottom, Front, Back
    struct TextureCubeSourceDesc
    {
        std::array<std::string, 6> facePaths;
        bool sRGB = true;
    };
} // namespace ChikaEngine::Resource
