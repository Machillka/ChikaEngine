
#include "ChikaEngine/ResourceSystem.h"
#include "ChikaEngine/MaterialImporter.h"
#include "ChikaEngine/MeshImporter.h"
#include "ChikaEngine/Resource/MaterialPool.h"
#include "ChikaEngine/Resource/Mesh.h"
#include "ChikaEngine/Resource/MeshPool.h"
#include "ChikaEngine/Resource/Shader.h"
#include "ChikaEngine/Resource/ShaderPool.h"
#include "ChikaEngine/Resource/TextureCubePool.h"
#include "ChikaEngine/Resource/TexturePool.h"
#include "ChikaEngine/ResourceTypes.h"
#include "ChikaEngine/ShaderImporter.h"
#include "ChikaEngine/TextureCubeImporter.h"
#include "ChikaEngine/TextureImporter.h"
#include "ChikaEngine/debug/log_macros.h"
#include "nlohmann/json_fwd.hpp"
#include <filesystem>
#include <string>

namespace ChikaEngine::Resource
{

    namespace fs = std::filesystem;
    using namespace ChikaEngine::Render;

    void ResourceSystem::Init(LocalSettingsContext& ctx)
    {
        _config.assetRoot = ctx.assetRoot;
        LOG_INFO("Resource System", "Init assetRoot={}", _config.assetRoot);
    }

    void ResourceSystem::Shutdown()
    {
        std::lock_guard<std::mutex> lk(_cacheMutex);
        _meshCache.clear();
        _textureCache.clear();
        _shaderCache.clear();
        _materialCache.clear();
        _textureCubeCache.clear();
        // TODO: shutdown pool
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
        MaterialHandle handle = MaterialPool::Create(matCPU);
        {
            std::lock_guard lk(_cacheMutex);
            _materialCache[fullPath] = handle;
        }
        return handle;
    }

    TextureCubeHandle ResourceSystem::LoadTextureCube(const TextureCubeSourceDesc& desc)
    {
        TextureCubeSourceDesc normalizedDesc = desc;
        for (auto& p : normalizedDesc.facePaths)
            p = NormalizePath(p);

        std::string key = MakeTextureCubeKey(normalizedDesc);

        {
            std::lock_guard lk(_cacheMutex);
            if (auto it = _textureCubeCache.find(key); it != _textureCubeCache.end())
            {
                return it->second;
            }
        }

        auto cpuData = Importer::TextureCubeImporter::Load(normalizedDesc);

        std::array<const void*, 6> rawDataPtrs;
        for (int i = 0; i < 6; ++i)
        {
            rawDataPtrs[i] = cpuData.facesPixels[i].data();
        }

        TextureCubeHandle handle = Render::TextureCubePool::Create(cpuData.width, cpuData.height, cpuData.channels, rawDataPtrs, cpuData.sRGB);

        {
            std::lock_guard lk(_cacheMutex);
            _textureCubeCache[key] = handle;
        }

        LOG_INFO("ResourceSystem", "TextureCube loaded via Desc. Handle={}", handle);
        return handle;
    }

    LocalSettingsContext ResourceSystem::LoadSettings(const std::string& path)
    {
        LocalSettingsContext ctx = LocalSettingsContext{};

        bool needWrite = false; // 如果读取失败,就重载一个进去
        if (!fs::exists(path))
            LOG_WARN("Resource System", "Local Setting Path doesn't exist!!!");
        try
        {
            std::ifstream i(path);
            nlohmann::json j;
            i >> j;

            // 读取 machineID
            if (j.contains("machineID"))
            {
                ctx.machineID = j["machineID"];
                if (ctx.machineID > 1023)
                {
                    LOG_WARN("Resource", "Invalid machineID in config. Regenerating...");
                    needWrite = true; // ID 非法，需要重置
                }
            }
            else
            {
                needWrite = true; // 缺字段，需要补
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Resource", "Failed to parse settings: {}. Using defaults.", e.what());
            needWrite = true;
        }

        if (needWrite)
        {
            // 生成随机 machineID
            if (ctx.machineID == 0 || ctx.machineID > 1023)
            {
                ctx.machineID = GenerateRandomMachineID();
            }
            if (ctx.assetRoot.empty())
                ctx.assetRoot = "Assets/";

            // 回写文件
            nlohmann::json j;
            // 如果文件存在，先读取旧内容防止覆盖其他字段
            if (fs::exists(path))
            {
                try
                {
                    std::ifstream i(path);
                    i >> j;
                }
                catch (...)
                {
                }
            }

            j["machineID"] = ctx.machineID;

            std::ofstream o(path);
            o << j.dump(4);
            LOG_INFO("Resource", "Saved local settings to {}", path);
        }

        return ctx;
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

    uint32_t ResourceSystem::GenerateRandomMachineID() const
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(0, 1023);
        return dist(gen);
    }

    std::string ResourceSystem::MakeTextureCubeKey(const TextureCubeSourceDesc& desc) const
    {
        std::stringstream ss;
        for (const auto& p : desc.facePaths)
        {
            ss << NormalizePath(p) << "|";
        }
        // 用 1, 0 表示是否有 sRGB
        ss << (desc.sRGB ? "1" : "0");
        return ss.str();
    }
} // namespace ChikaEngine::Resource