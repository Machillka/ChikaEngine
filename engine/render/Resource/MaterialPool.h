#pragma once
#include "render/Resource/Material.h"
#include "render/rhi/RHIDevice.h"
#include "render/rhi/RHIResources.h"

#include <array>
#include <vector>
namespace ChikaEngine::Render
{
    struct RHIMaterial
    {
        const IRHIPipeline* pipeline = nullptr;
        const IRHITexture2D* texture = nullptr;
        std::array<float, 4> albedoColor;
    };

    class MaterialPool
    {
      public:
        static void Init(IRHIDevice* device);
        static MaterialHandle Create(const Material& mat, const unsigned char* texDat, int width, int height);
        static const RHIMaterial& Get(MaterialHandle handle);
        static void Reset();

      private:
        static IRHIDevice* _device;
        static std::vector<RHIMaterial> _materials;
    };

} // namespace ChikaEngine::Render