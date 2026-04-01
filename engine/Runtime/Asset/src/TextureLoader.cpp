#include "ChikaEngine/TextureLoader.hpp"
#include <stb_image.h>

namespace ChikaEngine::Asset
{

    std::unique_ptr<TextureData> TextureLoader::Load(const std::string& path)
    {
        int w, h, c;
        stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &c, 4);

        auto tex = std::make_unique<TextureData>();
        tex->path = path;
        tex->width = w;
        tex->height = h;
        tex->channels = 4;
        tex->pixels.assign(pixels, pixels + w * h * 4);

        stbi_image_free(pixels);
        return tex;
    }
} // namespace ChikaEngine::Asset