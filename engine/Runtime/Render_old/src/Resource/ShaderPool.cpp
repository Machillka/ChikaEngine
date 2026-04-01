
#include "ChikaEngine/Resource/ShaderPool.h"
#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/debug/log_macros.h"
#include <stdexcept>

namespace ChikaEngine::Render
{
    IRHIDevice* ShaderPool::_device = nullptr;
    std::vector<Shader> ShaderPool::_shaders;
    std::vector<RHIShader> ShaderPool::_rhiShaders;

    void ShaderPool::Init(IRHIDevice* device)
    {
        _device = device;
    }

    ShaderHandle ShaderPool::Create(const Shader& shader)
    {
        if (!_device)
        {
            LOG_ERROR("ShaderPool", "ShaderPool::Init must be called before Create");
            throw std::runtime_error("ShaderPool not initialized");
        }
        auto* pipeline = _device->CreatePipeline(shader.vertexSource.c_str(), shader.fragmentSource.c_str());
        if (pipeline == nullptr)
        {
            LOG_ERROR("ShaderPool", "Failed to create pipeline");
            throw std::runtime_error("Error in ShaderPool");
        }
        RHIShader rhiShader{};
        rhiShader.pipeline = pipeline;

        // store CPU source and compiled pipeline
        Shader src = shader;
        _shaders.push_back(src);
        _rhiShaders.push_back(rhiShader);

        uint32_t index = static_cast<uint32_t>(_shaders.size() - 1);
        ShaderHandle h = Core::THandle<struct ShaderTag>::FromParts(index, 0);
        LOG_INFO("ShaderPool", "Created shader index={} total={}", index, _shaders.size());
        return h;
    }
    void ShaderPool::Reset()
    {
        _device = nullptr;
        _shaders.clear();
    }
    const Shader& ShaderPool::GetData(ShaderHandle handle)
    {
        return _shaders[handle.GetIndex()];
    }

    const RHIShader& ShaderPool::GetRHI(ShaderHandle handle)
    {
        return _rhiShaders[handle.GetIndex()];
    }
} // namespace ChikaEngine::Render
