#pragma once

#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/RHI/RHIResources.h"
#include "ChikaEngine/Resource/Material.h"
#include "ChikaEngine/Resource/Shader.h"
#include <vector>

namespace ChikaEngine::Render
{
    struct RHIShader
    {
        IRHIPipeline* pipeline = nullptr;
    };

    class ShaderPool
    {
      public:
        static void Init(IRHIDevice* device);
        static ShaderHandle Create(const Shader& shader);
        static const RHIShader& Get(ShaderHandle handle);
        static void Reset();

      private:
        static IRHIDevice* _device;
        static std::vector<RHIShader> _shaders;
    };
} // namespace ChikaEngine::Render
