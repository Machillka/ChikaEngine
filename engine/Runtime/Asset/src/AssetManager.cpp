#include "ChikaEngine/AssetHandle.hpp"
#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/MaterialLoader.hpp"
#include "ChikaEngine/MeshLoader.hpp"
#include "ChikaEngine/ShaderLoader.hpp"
#include "ChikaEngine/TextureLoader.hpp"
#include "ChikaEngine/ShaderTemplateLoader.hpp"
#include <ChikaEngine/AssetManager.hpp>
#include <string>

namespace ChikaEngine::Asset
{

    MeshHandle AssetManager::LoadMesh(const std::string& path)
    {
        if (m_meshPathCache.contains(path))
            return m_meshPathCache[path];
        auto data = MeshLoader::Load(path);
        auto handle = m_meshes.Create(*data);
        m_meshPathCache[path] = handle;
        return handle;
    }

    TextureHandle AssetManager::LoadTexture(const std::string& path)
    {
        if (m_texturePathCache.contains(path))
            return m_texturePathCache[path];
        auto data = TextureLoader::Load(path);
        auto handle = m_textures.Create(*data);
        m_texturePathCache[path] = handle;
        return handle;
    }

    ShaderHandle AssetManager::LoadShader(const std::string& path)
    {
        if (m_shaderPathCache.contains(path))
            return m_shaderPathCache[path];
        auto data = ShaderLoader::Load(path);
        auto handle = m_shaders.Create(*data);
        m_shaderPathCache[path] = handle;
        return handle;
    }

    ShaderTemplateHandle AssetManager::LoadShaderTemplate(const std::string& path)
    {
        if (m_shaderTemplatePathCache.contains(path))
            return m_shaderTemplatePathCache[path];
        auto data = ShaderTemplateLoader::Load(path);
        auto handle = m_shaderTemplates.Create(*data);
        m_shaderTemplatePathCache[path] = handle;
        return handle;
    }

    MaterialHandle AssetManager::LoadMaterial(const std::string& path)
    {
        if (m_materialPathCache.contains(path))
            return m_materialPathCache[path];
        auto data = MaterialLoader::Load(path);
        auto handle = m_materials.Create(*data);
        m_materialPathCache[path] = handle;
        return handle;
    }

    const MeshData* AssetManager::GetMesh(MeshHandle h) const
    {
        return m_meshes.Get(h);
    }

    const TextureData* AssetManager::GetTexture(TextureHandle h) const
    {
        return m_textures.Get(h);
    }

    const ShaderData* AssetManager::GetShader(ShaderHandle h) const
    {
        return m_shaders.Get(h);
    }

    const ShaderTemplateData* AssetManager::GetShaderTemplate(ShaderTemplateHandle h) const
    {
        return m_shaderTemplates.Get(h);
    }

    const MaterialData* AssetManager::GetMaterial(MaterialHandle h) const
    {
        return m_materials.Get(h);
    }

} // namespace ChikaEngine::Asset