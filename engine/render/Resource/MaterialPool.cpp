#include "MaterialPool.h"

#include "render/Resource/ShaderPool.h"
namespace ChikaEngine::Render
{
    IRHIDevice* MaterialPool::_device = nullptr;
    std::vector<RHIMaterial> MaterialPool::_materials;

    void MaterialPool::Init(IRHIDevice* device)
    {
        _device = device;
    }

    MaterialHandle MaterialPool::Create(const Material& material, const unsigned char* texData, int width, int height)
    {
        auto& shader = ShaderPool::Get(material.shaderHandle);
        auto* texture = _device->CreateTexture2D(width, height, texData);
        RHIMaterial renderMaterial;
        renderMaterial.pipeline = shader.pipeline;
        renderMaterial.texture = texture;
        _materials.push_back(renderMaterial);
        return static_cast<MaterialHandle>(_materials.size() - 1);
    }
    void MaterialPool::Reset()
    {
        _device = nullptr;
        _materials.clear();
    }
    const RHIMaterial& MaterialPool::Get(MaterialHandle handle)
    {
        return _materials[handle];
    }
} // namespace ChikaEngine::Render