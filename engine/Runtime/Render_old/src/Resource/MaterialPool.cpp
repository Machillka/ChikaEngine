
#include "ChikaEngine/Resource/MaterialPool.h"
#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/Resource/ShaderPool.h"
#include "ChikaEngine/Resource/TexturePool.h"
#include "ChikaEngine/Resource/TextureCubePool.h"
#include "ChikaEngine/debug/log_macros.h"
namespace ChikaEngine::Render
{
    IRHIDevice* MaterialPool::_device = nullptr;
    std::vector<Material> MaterialPool::_materials;
    std::vector<RHIMaterial> MaterialPool::_rhiMaterials;

    void MaterialPool::Init(IRHIDevice* device)
    {
        _device = device;
    }

    MaterialHandle MaterialPool::Create(const Material& material)
    {
        RHIMaterial matGPU{};

        // get compiled pipeline from shader pool
        const auto& shaderRHI = ShaderPool::GetRHI(material.shaderHandle);
        matGPU.pipeline = shaderRHI.pipeline;

        // resolve textures
        for (auto& [name, texHandle] : material.textures)
        {
            const auto& texRHI = TexturePool::GetRHI(texHandle);
            matGPU.textures[name] = (IRHITexture2D*)texRHI.texture;
        }
        for (auto& [name, cubeHandle] : material.cubemaps)
        {
            const auto& cubeData = TextureCubePool::Get(cubeHandle);
            matGPU.cubemaps[name] = (IRHITextureCube*)cubeData.texture;
        }

        matGPU.uniformFloats = material.uniformFloats;
        matGPU.uniformVec4s = material.uniformVec4s;
        matGPU.uniformVec3s = material.uniformVec3s;
        // material.uniformMat4s uses std::array<float,16> for mat4 storage
        matGPU.uniformMat4s.clear();
        for (const auto& [k, v] : material.uniformMat4s)
        {
            Math::Mat4 m{};
            // Copy 16 floats into Math::Mat4 (row-major assumed)
            for (int i = 0; i < 16; ++i)
                m.m[i] = v[i];
            matGPU.uniformMat4s[k] = m;
        }

        // store CPU and GPU
        _materials.push_back(material);
        _rhiMaterials.push_back(matGPU);

        uint32_t index = static_cast<uint32_t>(_materials.size() - 1);
        MaterialHandle h = Core::THandle<struct MaterialTag>::FromParts(index, 0);
        LOG_INFO("MaterialPool", "Created material index={} textures={} total={}", index, matGPU.textures.size(), _materials.size());

        return h;
    }

    void MaterialPool::Reset()
    {
        _device = nullptr;
        _materials.clear();
        _rhiMaterials.clear();
    }

    // 把材质上的各种参数绑定到shader上 (RHI-agnostic)
    void MaterialPool::Apply(MaterialHandle handle)
    {
        const auto& mat = GetRHI(handle);

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

        slot = 0;
        for (const auto& [name, cubePtr] : mat.cubemaps)
        {
            mat.pipeline->SetUniformTextureCube(name.c_str(), (IRHITextureCube*)cubePtr, slot);
            ++slot;
        }

        // LOG_INFO("MaterialPool", "vec3 count = {}", mat.uniformVec3s.size());
        // for (const auto& [name, arr] : mat.uniformVec3s)
        // {
        // LOG_INFO("MaterialPool", "vec3 uniform {} = ({}, {}, {})", name, arr[0], arr[1], arr[2]);
        // }
    }

    const Material& MaterialPool::GetData(MaterialHandle handle)
    {
        return _materials[handle.GetIndex()];
    }

    RHIMaterial& MaterialPool::GetRHI(MaterialHandle handle)
    {
        return _rhiMaterials[handle.GetIndex()];
    }
} // namespace ChikaEngine::Render