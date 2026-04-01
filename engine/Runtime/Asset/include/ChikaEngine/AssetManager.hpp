/*!
 * @file AssetManager.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 资产的概念, 底层读取数据 所以起成 Asset 空间
 *
 * 数据流动方向： 硬盘 -> CPU
 * @version 0.1
 * @date 2026-03-27
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "AssetHandle.hpp"
#include "AssetLayouts.hpp"
#include "ChikaEngine/base/SlotMap.h"
#include <string>
#include <unordered_map>
namespace ChikaEngine::Asset
{

    class AssetManager
    {
      public:
        // load from disk
        TextureHandle LoadTexture(const std::string& path);
        MeshHandle LoadMesh(const std::string& path);
        ShaderHandle LoadShader(const std::string& path);
        MaterialHandle LoadMaterial(const std::string& path);
        ShaderTemplateHandle LoadShaderTemplate(const std::string& path);
        template <typename HandleType> MaterialHandle Load(const std::string& path) {}

      public:
        // 查询
        const TextureData* GetTexture(TextureHandle h) const;
        const MeshData* GetMesh(MeshHandle h) const;
        const ShaderData* GetShader(ShaderHandle h) const;
        const MaterialData* GetMaterial(MaterialHandle h) const;
        const ShaderTemplateData* GetShaderTemplate(ShaderTemplateHandle h) const;

      private:
        Core::SlotMap<TextureHandle, TextureData> m_textures;
        Core::SlotMap<MeshHandle, MeshData> m_meshes;
        Core::SlotMap<ShaderHandle, ShaderData> m_shaders;
        Core::SlotMap<MaterialHandle, MaterialData> m_materials;
        Core::SlotMap<ShaderTemplateHandle, ShaderTemplateData> m_shaderTemplates;

        // 路径缓存（避免重复加载）
        std::unordered_map<std::string, TextureHandle> m_texturePathCache;
        std::unordered_map<std::string, MeshHandle> m_meshPathCache;
        std::unordered_map<std::string, ShaderHandle> m_shaderPathCache;
        std::unordered_map<std::string, MaterialHandle> m_materialPathCache;
        std::unordered_map<std::string, ShaderTemplateHandle> m_shaderTemplatePathCache;
    };
} // namespace ChikaEngine::Asset