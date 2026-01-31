#include "include/ResourceSystem.h"

#include "debug/log_macros.h"
#include "include/ResourceTypes.h"
#include "render/Resource/Material.h"
#include "render/Resource/MaterialPool.h"
#include "render/Resource/Mesh.h"
#include "render/Resource/MeshPool.h"
#include "render/Resource/Shader.h"
#include "render/Resource/ShaderPool.h"
#include "render/Resource/TexturePool.h"
#include "resource_importer/include/MaterialImporter.h"
#include "resource_importer/include/MeshImporter.h"
#include "resource_importer/include/ShaderImporter.h"
#include "resource_importer/include/TextureImporter.h"

#include <filesystem>
#include <string>

namespace ChikaEngine::Resource
{

    namespace fs = std::filesystem;
    using namespace ChikaEngine::Render;

    void ResourceSystem::Init(const ResourceConfig& cfg)
    {
        _config = cfg;
        LOG_INFO("Resource System", "Init assetRoot={}", _config.assetRoot);
    }

    void ResourceSystem::Shutdown()
    {
        std::lock_guard<std::mutex> lk(_cacheMutex);
        _meshCache.clear();
        _textureCache.clear();
        _shaderCache.clear();
        _materialCache.clear();
        LOG_INFO("ResourceSystem", "Shutdown");
    }

    std::string ResourceSystem::NormalizePath(const std::string& path) const
    {
        fs::path p = fs::path(_config.assetRoot) / path;
        // 尝试规范化路径，但不会因为文件不存在而抛异常
        return fs::weakly_canonical(p).string();
    }

    std::string ResourceSystem::MakeShaderKey(const ShaderSourceDesc& desc)
    {
        return desc.vertexPath + "|" + desc.fragmentPath;
    }

    // 提供相对路径
    MeshHandle ResourceSystem::LoadMesh(const std::string& path)
    {
        std::string fullPath = NormalizePath(path);
        LOG_INFO("ResourceSystem", "LoadMesh request path='{}' resolved='{}'", path, fullPath);
        {
            std::lock_guard lk(_cacheMutex);
            if (auto it = _meshCache.find(fullPath); it != _meshCache.end())
                return it->second;
        }
        Render::Mesh cpuMesh = Importer::MeshImporter::Load(fullPath);
        MeshHandle handle = MeshPool::Create(cpuMesh);
        {
            std::lock_guard lk(_cacheMutex);
            _meshCache[fullPath] = handle;
        }

        return handle;
    }

    TextureHandle ResourceSystem::LoadTexture(const std::string& path, bool sRGB)
    {
        std::string fullPath = NormalizePath(path);
        {
            std::lock_guard lk(_cacheMutex);
            if (auto it = _textureCache.find(fullPath); it != _textureCache.end())
                return it->second;
        }

        auto texData = Importer::TextureImporter::Load(fullPath, sRGB);
        LOG_INFO("ResourceSystem", "Creating texture from loaded data width={} height={} channels={}", texData.width, texData.height, texData.channels);
        TextureHandle handle = TexturePool::Create(texData.width, texData.height, texData.channels, texData.pixels.data(), texData.sRGB);
        {
            std::lock_guard lk(_cacheMutex);
            _textureCache[fullPath] = handle;
        }
        LOG_INFO("ResourceSystem", "Texture loaded and created handle={}", handle);
        return handle;
    }

    ShaderHandle ResourceSystem::LoadShader(const ShaderSourceDesc& desc)
    {
        ShaderSourceDesc normalized = desc;
        normalized.vertexPath = NormalizePath(desc.vertexPath);
        normalized.fragmentPath = NormalizePath(desc.fragmentPath);
        std::string key = MakeShaderKey(normalized);
        {
            std::lock_guard lk(_cacheMutex);
            if (auto it = _shaderCache.find(key); it != _shaderCache.end())
                return it->second;
        }
        auto src = Importer::ShaderImporter::Load(normalized);
        Shader shaderCPU{src.vertex, src.fragment};
        ShaderHandle handle = ShaderPool::Create(shaderCPU);
        {
            std::lock_guard lk(_cacheMutex);
            _shaderCache[key] = handle;
        }
        return handle;
    }
    MaterialHandle ResourceSystem::LoadMaterial(const std::string& path)
    {
        std::string fullPath = NormalizePath(path);
        LOG_INFO("ResourceSystem", "LoadMaterial request path='{}' resolved='{}'", path, fullPath);
        {
            std::lock_guard lk(_cacheMutex);
            if (auto it = _materialCache.find(fullPath); it != _materialCache.end())
                return it->second;
        }
        Material matCPU = Importer::MaterialImporter::Load(fullPath, *this);
        LOG_INFO("Fuck", "FUckl");
        MaterialHandle handle = MaterialPool::Create(matCPU);
        {
            std::lock_guard lk(_cacheMutex);
            _materialCache[fullPath] = handle;
        }
        return handle;
    }

    bool ResourceSystem::HasMesh(const std::string& path) const
    {
        std::string fullPath = NormalizePath(path);
        std::lock_guard lk(_cacheMutex);
        return _meshCache.contains(fullPath);
    }

    bool ResourceSystem::HasTexture(const std::string& path) const
    {
        std::string fullPath = NormalizePath(path);
        std::lock_guard lk(_cacheMutex);
        return _textureCache.contains(fullPath);
    }

    bool ResourceSystem::HasShader(const ShaderSourceDesc& desc) const
    {
        ShaderSourceDesc normalized = desc;
        normalized.vertexPath = NormalizePath(desc.vertexPath);
        normalized.fragmentPath = NormalizePath(desc.fragmentPath);
        std::string key = MakeShaderKey(normalized);
        std::lock_guard lk(_cacheMutex);
        return _shaderCache.contains(key);
    }

    bool ResourceSystem::HasMaterial(const std::string& path) const
    {
        std::string full = const_cast<ResourceSystem*>(this)->NormalizePath(path);
        std::lock_guard lk(_cacheMutex);
        return _materialCache.contains(full);
    }

} // namespace ChikaEngine::Resource