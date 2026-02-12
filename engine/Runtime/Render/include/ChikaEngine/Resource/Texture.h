#pragma once

#include <cstdint>
namespace ChikaEngine::Render
{
    struct Texture
    {
        int width = 0;
        int height = 0;
        int channels = 4;
    };

    using TextureHandle = std::uint32_t;
} // namespace ChikaEngine::Render