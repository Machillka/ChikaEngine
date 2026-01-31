#pragma once

#include "render/Resource/Texture.h"
#include "render/rhi/RHIDevice.h"
#include "render/rhi/RHIResources.h"

#include <vector>

namespace ChikaEngine::Render
{
    struct RHITexture2D
    {
        IRHITexture2D* texture = nullptr;
        int width = 0;
        int height = 0;
        int channels = 1;
        bool sRGB = true;
    };

    class TexturePool
    {
      public:
        static void Init(IRHIDevice* device);
        static void Reset();

        // 从 CPU 纹理数据创建 GPU 纹理，返回句柄
        static TextureHandle Create(int width, int height, int channels, const unsigned char* pixels, bool sRGB);
        static const RHITexture2D& Get(TextureHandle handle);

      private:
        static IRHIDevice* _device;
        static std::vector<RHITexture2D> _textures;
    };
} // namespace ChikaEngine::Render