#pragma once
#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/RHI/RHIResources.h"
#include "ChikaEngine/Resource/Material.h"

#include <array>
#include <unordered_map>
#include <vector>
namespace ChikaEngine::Render
{
    struct RHIMaterial
    {
        IRHIPipeline* pipeline = nullptr;
        std::unordered_map<std::string, IRHITexture2D*> textures;
        std::unordered_map<std::string, IRHITextureCube*> cubemaps;
        std::unordered_map<std::string, float> uniformFloats;
        std::unordered_map<std::string, std::array<float, 3>> uniformVec3s;
        std::unordered_map<std::string, std::array<float, 4>> uniformVec4s;
        std::unordered_map<std::string, Math::Mat4> uniformMat4s;
    };

    class MaterialPool
    {
      public:
        static void Init(IRHIDevice* device);
        static MaterialHandle Create(const Material& material);
        static RHIMaterial& Get(MaterialHandle handle);
        static void Reset();
        static void Apply(MaterialHandle handle);

      private:
        static IRHIDevice* _device;
        static std::vector<RHIMaterial> _materials;
    };

} // namespace ChikaEngine::Render