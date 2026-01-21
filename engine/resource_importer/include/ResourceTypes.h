#pragma once
#include <string>

namespace ChikaEngine::Resource
{
    using MeshHandle = uint32_t;
    using TextureHandle = uint32_t;
    using ShaderHandle = uint32_t;
    using MaterialHandle = uint32_t;

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
} // namespace ChikaEngine::Resource
