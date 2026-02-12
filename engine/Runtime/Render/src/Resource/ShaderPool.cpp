
#include "ChikaEngine/Resource/ShaderPool.h"
#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/debug/log_macros.h"
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

        _shaders.push_back(rhiShader);
        ShaderHandle h = static_cast<ShaderHandle>(_shaders.size() - 1);
        LOG_INFO("ShaderPool", "Created shader handle={} total={}", h, _shaders.size());
        return h;
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
