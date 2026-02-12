#pragma once
#include "ResourceTypes.h"

#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>

namespace ChikaEngine::Resource
{
    /*
    ==== 流程:
            1. 先尝试读取 localsettings,里面存储配置数据
            2. 把 localsettings 传入 init, init 会从里面拿取自己需要的字段
            3. 继续使用
    */

    // 解析 settings 返回需要的数据,进行一个耦的解
    struct LocalSettingsContext
    {
        std::uint32_t machineID = 0;
        std::string assetRoot = "Assets/";
    };

    // 打包 资源系统自身需要的所有字段
    // WORKFLOW: 在此处添加需要字段,并且在 Init 中读取
    struct ResourceConfig
    {
        std::string assetRoot = "";
    };

    class ResourceSystem
    {
      public:
        static ResourceSystem& Instance()
        {
            static ResourceSystem instance;
            return instance;
        }

        void Init(LocalSettingsContext& ctx);
        void Shutdown();

        // WORKFLOW: 往其中添加新的资源读写
        MeshHandle LoadMesh(const std::string& path);
        TextureHandle LoadTexture(const std::string& path, bool sRGB = true);
        ShaderHandle LoadShader(const ShaderSourceDesc& desc);
        MaterialHandle LoadMaterial(const std::string& path);
        void TryLoadSettings(const std::string& path, LocalSettingsContext& ctx);

        bool HasMesh(const std::string& path) const;
        bool HasTexture(const std::string& path) const;
        bool HasShader(const ShaderSourceDesc& desc) const;
        bool HasMaterial(const std::string& path) const;

        // void Reload(const std::string& path);

        std::string NormalizePath(const std::string& path) const;

      private:
        ResourceSystem() = default;
        ~ResourceSystem() = default;
        ResourceConfig _config = {};

        // 资产缓冲
        std::unordered_map<std::string, MeshHandle> _meshCache;
        std::unordered_map<std::string, TextureHandle> _textureCache;
        std::unordered_map<std::string, ShaderHandle> _shaderCache;
        std::unordered_map<std::string, MaterialHandle> _materialCache;

        // 上锁
        mutable std::mutex _cacheMutex;
        static std::string MakeShaderKey(const ShaderSourceDesc& desc);

        std::uint32_t GenerateRandomMachineID() const;
    };
} // namespace ChikaEngine::Resource