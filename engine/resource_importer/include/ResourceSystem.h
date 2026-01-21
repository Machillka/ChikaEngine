#pragma once
#include "ResourceTypes.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace ChikaEngine::Resource
{
    struct ResourceConfig
    {
        std::string assetRoot = "Assets/";
        // TODO: 实现热更
    };

    class ResourceSystem
    {
      public:
        ResourceSystem() = default;
        ~ResourceSystem() = default;
        void Init(const ResourceConfig& cfg);
        void Shutdown();

        // WORKFLOW: 往其中添加新的资源读写
        MeshHandle LoadMesh(const std::string& path);
        TextureHandle LoadTexture(const std::string& path, bool sRGB = true);
        ShaderHandle LoadShader(const ShaderSourceDesc& desc);
        MaterialHandle LoadMaterial(const std::string& path);

        bool HasMesh(const std::string& path) const;
        bool HasTexture(const std::string& path) const;
        bool HasShader(const ShaderSourceDesc& desc) const;
        bool HasMaterial(const std::string& path) const;

        // void Reload(const std::string& path);

        std::string NormalizePath(const std::string& path) const;

      private:
        ResourceConfig _config;

        // 资产缓冲
        std::unordered_map<std::string, MeshHandle> _meshCache;
        std::unordered_map<std::string, TextureHandle> _textureCache;
        std::unordered_map<std::string, ShaderHandle> _shaderCache;
        std::unordered_map<std::string, MaterialHandle> _materialCache;
        // 上锁
        mutable std::mutex _cacheMutex;
        static std::string MakeShaderKey(const ShaderSourceDesc& desc);
    };
} // namespace ChikaEngine::Resource