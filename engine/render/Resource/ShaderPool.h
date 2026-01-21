#pragma once
#include "render/Resource/Shader.h"
#include "render/rhi/RHIDevice.h"
#include "render/rhi/RHIResources.h"

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
