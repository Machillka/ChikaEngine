#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/debug/log_macros.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>

namespace ChikaEngine::Asset
{
    namespace
    {
        std::string LowerExtension(const std::filesystem::path& path)
        {
            std::string extension = path.extension().string();
            std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return extension;
        }
    } // namespace

    bool AssetDatabase::Initialize(const std::filesystem::path& assetRoot)
    {
        Shutdown();
        m_assetRoot = NormalizePath(assetRoot);
        std::error_code error;
        std::filesystem::create_directories(m_assetRoot, error);
        if (error)
        {
            LOG_ERROR("AssetDatabase", "Failed to create asset root {}: {}", m_assetRoot.string(), error.message());
            return false;
        }
        // 初始化的时候 递归遍历生成 guid 和 meta
        return Scan();
    }

    void AssetDatabase::Shutdown()
    {
        m_recordsByGuid.clear();
        m_guidByPath.clear();
        m_assetRoot.clear();
    }

    bool AssetDatabase::Scan(bool createMissingMeta)
    {
        if (m_assetRoot.empty())
            return false;

        m_recordsByGuid.clear();
        m_guidByPath.clear();

        std::error_code error;
        for (std::filesystem::recursive_directory_iterator it(m_assetRoot, error), end; it != end && !error; it.increment(error))
        {
            if (!it->is_regular_file() || IsIgnored(it->path()))
                continue;
            RegisterAsset(it->path(), createMissingMeta);
        }

        if (error)
        {
            LOG_ERROR("AssetDatabase", "Failed to scan {}: {}", m_assetRoot.string(), error.message());
            return false;
        }
        return true;
    }

    bool AssetDatabase::RegisterAsset(const std::filesystem::path& sourcePath, bool createMissingMeta)
    {
        const auto normalized = NormalizePath(sourcePath);
        if (!std::filesystem::is_regular_file(normalized) || IsIgnored(normalized))
            return false;

        AssetRecord record;
        record.sourcePath = normalized;
        record.metaPath = normalized.string() + ".meta";
        record.type = Classify(normalized);
        record.importer = DefaultImporterName(record.type);
        record.importedPath = normalized;

        std::error_code error;
        record.sourceWriteTime = std::filesystem::last_write_time(normalized, error);

        if (std::filesystem::exists(record.metaPath))
        {
            if (!LoadMeta(record.metaPath, record))
                return false;
            record.sourcePath = normalized;
            record.metaPath = normalized.string() + ".meta";
            record.sourceWriteTime = std::filesystem::last_write_time(normalized, error);
            SaveMeta(record);
        }
        else
        {
            if (!createMissingMeta)
                return false;
            record.guid = GenerateGuid();
            if (!SaveMeta(record))
                return false;
        }

        if (!record.guid.IsValid())
            return false;
        IndexRecord(std::move(record));
        return true;
    }

    const AssetRecord* AssetDatabase::FindByGuid(const AssetGuid& guid) const
    {
        const auto it = m_recordsByGuid.find(guid.value);
        return it == m_recordsByGuid.end() ? nullptr : &it->second;
    }

    const AssetRecord* AssetDatabase::FindByPath(const std::filesystem::path& path) const
    {
        const auto pathIt = m_guidByPath.find(NormalizePath(path).generic_string());
        return pathIt == m_guidByPath.end() ? nullptr : FindByGuid({ pathIt->second });
    }

    std::vector<AssetRecord> AssetDatabase::GetAllAssets() const
    {
        std::vector<AssetRecord> result;
        result.reserve(m_recordsByGuid.size());
        for (const auto& [guid, record] : m_recordsByGuid)
            result.push_back(record);
        return result;
    }

    bool AssetDatabase::UpdateImportResult(const AssetGuid& guid, const std::string& importer, const std::filesystem::path& importedPath)
    {
        const auto it = m_recordsByGuid.find(guid.value);
        if (it == m_recordsByGuid.end())
            return false;

        const auto normalizedImportedPath = NormalizePath(importedPath);
        if (it->second.importer == importer && it->second.importedPath == normalizedImportedPath)
            return true;

        it->second.importer = importer;
        it->second.importedPath = normalizedImportedPath;
        return SaveMeta(it->second);
    }

    std::vector<AssetGuid> AssetDatabase::PollChangedAssets()
    {
        std::vector<AssetGuid> changed;
        for (auto& [guid, record] : m_recordsByGuid)
        {
            std::error_code error;
            const auto currentWriteTime = std::filesystem::last_write_time(record.sourcePath, error);
            if (!error && currentWriteTime != record.sourceWriteTime)
            {
                record.sourceWriteTime = currentWriteTime;
                changed.push_back(record.guid);
            }
        }
        return changed;
    }

    AssetType AssetDatabase::Classify(const std::filesystem::path& path)
    {
        const std::string extension = LowerExtension(path);
        if (extension == ".gltf" || extension == ".glb" || extension == ".obj")
            return AssetType::Mesh;
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" || extension == ".hdr")
            return AssetType::Texture;
        if (extension == ".vert" || extension == ".frag" || extension == ".comp" || extension == ".geom" || extension == ".tesc" || extension == ".tese")
            return AssetType::ShaderSource;
        if (extension == ".spv")
            return AssetType::ShaderBinary;
        if (extension == ".py")
            return AssetType::Script;
        if (extension == ".json")
        {
            const std::string name = path.filename().string();
            return name.find(".template.") != std::string::npos ? AssetType::ShaderTemplate : AssetType::Material;
        }
        return AssetType::Unknown;
    }

    std::string AssetDatabase::AssetTypeName(AssetType type)
    {
        switch (type)
        {
        case AssetType::Mesh:
            return "mesh";
        case AssetType::Texture:
            return "texture";
        case AssetType::ShaderSource:
            return "shader_source";
        case AssetType::ShaderBinary:
            return "shader_binary";
        case AssetType::Material:
            return "material";
        case AssetType::ShaderTemplate:
            return "shader_template";
        case AssetType::Script:
            return "script";
        default:
            return "unknown";
        }
    }

    std::string AssetDatabase::DefaultImporterName(AssetType type)
    {
        return type == AssetType::ShaderSource ? "shader" : "passthrough";
    }

    std::filesystem::path AssetDatabase::NormalizePath(const std::filesystem::path& path)
    {
        std::error_code error;
        const auto absolute = std::filesystem::absolute(path, error);
        return (error ? path : absolute).lexically_normal();
    }

    AssetGuid AssetDatabase::GenerateGuid()
    {
        std::random_device random;
        std::array<uint32_t, 4> words{ random(), random(), random(), random() };
        std::ostringstream stream;
        for (const uint32_t word : words)
            stream << std::hex << std::setw(8) << std::setfill('0') << word;
        return { stream.str() };
    }

    bool AssetDatabase::IsIgnored(const std::filesystem::path& path)
    {
        const std::string extension = LowerExtension(path);
        if (extension == ".meta" || extension == ".pyc")
            return true;
        if (path.filename().string().ends_with(".reflection.json"))
            return true;
        if (extension == ".spv")
        {
            std::filesystem::path possibleSource = path;
            possibleSource.replace_extension();
            if (Classify(possibleSource) == AssetType::ShaderSource && std::filesystem::is_regular_file(possibleSource))
                return true;
        }
        for (const auto& part : path)
        {
            if (part == "__pycache__")
                return true;
        }
        return false;
    }

    bool AssetDatabase::LoadMeta(const std::filesystem::path& metaPath, AssetRecord& record) const
    {
        try
        {
            std::ifstream file(metaPath);
            const auto json = nlohmann::json::parse(file);
            record.guid.value = json.value("guid", "");
            record.importer = json.value("importer", record.importer);
            const std::filesystem::path imported = json.value("imported", record.sourcePath.generic_string());
            record.importedPath = NormalizePath(imported.is_relative() ? m_assetRoot / imported : imported);
            return record.guid.IsValid();
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("AssetDatabase", "Failed to read meta {}: {}", metaPath.string(), exception.what());
            return false;
        }
    }

    bool AssetDatabase::SaveMeta(const AssetRecord& record) const
    {
        try
        {
            std::error_code error;
            const auto relativeImportedPath = std::filesystem::relative(record.importedPath, m_assetRoot, error);
            const bool importedInsideRoot = !error && !relativeImportedPath.empty() && *relativeImportedPath.begin() != "..";
            const std::string serializedImportedPath = importedInsideRoot ? relativeImportedPath.generic_string() : record.importedPath.generic_string();
            nlohmann::json json{
                { "version", 1 }, { "guid", record.guid.value }, { "type", AssetTypeName(record.type) }, { "importer", record.importer }, { "imported", serializedImportedPath },
            };
            std::ofstream file(record.metaPath, std::ios::trunc);
            file << json.dump(2) << '\n';
            return file.good();
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR("AssetDatabase", "Failed to write meta {}: {}", record.metaPath.string(), exception.what());
            return false;
        }
    }

    void AssetDatabase::IndexRecord(AssetRecord record)
    {
        const std::string guid = record.guid.value;
        m_guidByPath[record.sourcePath.generic_string()] = guid;
        m_recordsByGuid[guid] = std::move(record);
    }
} // namespace ChikaEngine::Asset
