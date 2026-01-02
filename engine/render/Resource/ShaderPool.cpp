#include "ShaderPool.h"

#include <stdexcept>

namespace ChikaEngine::Render
{
    IRHIDevice* ShaderPool::_device = nullptr;
    std::vector<RHIShader> ShaderPool::_shaders;

    void ShaderPool::Init(IRHIDevice* device)
    {
        _device = device;
    }

    ShaderHandle ShaderPool::Create(const Shader& shader)
    {
        auto* pipeline = _device->CreatePipeline(shader.vertexSource.c_str(), shader.fragmentSource.c_str());
        if (pipeline == nullptr)
        {
            std::runtime_error("Error in ShaderPool");
        }
        RHIShader rhiShader;
        rhiShader.pipeline = pipeline;

        _shaders.push_back(rhiShader);
        return static_cast<ShaderHandle>(_shaders.size() - 1);
    }
    void ShaderPool::Reset()
    {
        _device = nullptr;
        _shaders.clear();
    }
    const RHIShader& ShaderPool::Get(ShaderHandle handle)
    {
        return _shaders[handle];
    }
} // namespace ChikaEngine::Render
