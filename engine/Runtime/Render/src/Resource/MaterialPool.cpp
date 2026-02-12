
#include "ChikaEngine/Resource/MaterialPool.h"
#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/Resource/ShaderPool.h"
#include "ChikaEngine/Resource/TexturePool.h"
#include "ChikaEngine/debug/log_macros.h"
namespace ChikaEngine::Render
{
    IRHIDevice* MaterialPool::_device = nullptr;
    std::vector<RHIMaterial> MaterialPool::_materials;

    void MaterialPool::Init(IRHIDevice* device)
    {
        _device = device;
    }

    MaterialHandle MaterialPool::Create(const Material& material)
    {
        RHIMaterial matGPU{};

        auto& shader = ShaderPool::Get(material.shaderHandle);
        matGPU.pipeline = shader.pipeline;

        for (auto& [name, texHandle] : material.textures)
        {
            const auto& tex = TexturePool::Get(texHandle);
            matGPU.textures[name] = tex.texture;
        }
        matGPU.uniformFloats = material.uniformFloats;
        matGPU.uniformVec4s = material.uniformVec4s;
        matGPU.uniformVec3s = material.uniformVec3s;

        _materials.push_back(matGPU);

        MaterialHandle h = static_cast<MaterialHandle>(_materials.size() - 1);
        LOG_INFO("MaterialPool", "Created material handle={} textures={} total={}", h, matGPU.textures.size(), _materials.size());

        return h;
    }

    void MaterialPool::Reset()
    {
        _device = nullptr;
        _materials.clear();
    }

    // 把材质上的各种参数绑定到shader上 (RHI-agnostic)
    void MaterialPool::Apply(MaterialHandle handle)
    {
        const auto& mat = Get(handle);

        if (mat.pipeline == nullptr)
            return;

        // bind pipeline
        mat.pipeline->Bind();

        // set float uniforms
        for (const auto& [name, val] : mat.uniformFloats)
        {
            mat.pipeline->SetUniformFloat(name.c_str(), val);
        }

        for (const auto& [name, arr] : mat.uniformVec3s)
        {
            mat.pipeline->SetUniformVec3(name.c_str(), arr);
        }

        // set vec4 uniforms
        for (const auto& [name, arr] : mat.uniformVec4s)
        {
            mat.pipeline->SetUniformVec4(name.c_str(), arr);
        }

        for (const auto& [name, mat4] : mat.uniformMat4s)
        {
            mat.pipeline->SetUniformMat4(name.c_str(), mat4);
        }

        // bind textures to successive texture units
        int slot = 0;
        for (const auto& [name, texPtr] : mat.textures)
        {
            mat.pipeline->SetUniformTexture(name.c_str(), texPtr, slot);
            ++slot;
        }

        // LOG_INFO("MaterialPool", "vec3 count = {}", mat.uniformVec3s.size());
        // for (const auto& [name, arr] : mat.uniformVec3s)
        // {
        // LOG_INFO("MaterialPool", "vec3 uniform {} = ({}, {}, {})", name, arr[0], arr[1], arr[2]);
        // }
    }

    RHIMaterial& MaterialPool::Get(MaterialHandle handle)
    {
        return _materials[handle];
    }
} // namespace ChikaEngine::Render