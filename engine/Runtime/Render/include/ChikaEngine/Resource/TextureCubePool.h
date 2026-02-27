#pragma once

#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/RHI/RHIResources.h"
#include <cstdint>
namespace ChikaEngine::Render
{
    using TextureCubeHandle = std::uint32_t;

    struct RHITextureCubeData
    {
        IRHITextureCube* texture;
    };

    class TextureCubePool
    {
      public:
        static void Init(IRHIDevice* device);
        static TextureCubeHandle Create(int width, int height, int channels, const std::array<const void*, 6>& facesData, bool sRGB);
        static const RHITextureCubeData& Get(TextureCubeHandle handle);
        static void Reset();

      private:
        static IRHIDevice* _device;
        static std::vector<RHITextureCubeData> _textures;
    };
} // namespace ChikaEngine::Render