#pragma once

#include "debug/log_macros.h"
#include "stb_image.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ChikaEngine::Resource::Importer
{
    // 方便直接和 pool 对接的 cpu 端数据类型
    struct TextureData
    {
        int width = 0;
        int height = 0;
        int channels = 1;
        bool sRGB = true;
        std::vector<unsigned char> pixels;
    };

    struct TextureImporter
    {
        static TextureData Load(const std::string& path, bool sRGB)
        {
            int w, h, ch;
            unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);

            if (!data)
            {
                LOG_ERROR("Texture Importer", "Texture load failed: {}", path);
                throw std::runtime_error("Texture load failed: " + path);
            }

            LOG_INFO("Texture Importer", "Loaded texture {} size={}x{} channels={}", path, w, h, ch);

            TextureData texture;
            texture.width = w;
            texture.height = h;
            texture.channels = ch;
            texture.sRGB = sRGB;
            texture.pixels.assign(data, data + w * h * ch);
            stbi_image_free(data);
            return texture;
        }
    };
} // namespace ChikaEngine::Resource::Importer