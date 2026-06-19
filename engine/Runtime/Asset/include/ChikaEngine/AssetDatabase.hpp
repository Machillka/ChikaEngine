#pragma once

#include <compare>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ChikaEngine::Asset
{
    enum class AssetType
    {
        Unknown,
        Mesh,
        Texture,
        ShaderSource,
        ShaderBinary,
        Material,
        ShaderTemplate,
        Script,
        Scene,
    };

    /**
     * @brief 控制 AssetDatabase 初始化时允许执行的开发态行为。
     *
     * Packaged Runtime 会关闭目录创建、扫描和 meta 生成；Development Runtime
     * 与 Editor 则可按需开启这些能力。
     */
    struct AssetDatabaseCreateInfo
    {
        std::filesystem::path assetRoot = "Assets";
        bool createRoot = true;
        bool scanAssets = true;
        bool createMissingMeta = true;
    };

    struct AssetGuid
    {
        std::string value;

        bool IsValid() const
        {
            return !value.empty();
        }

        auto operator<=>(const AssetGuid&) const = default;
    };

    struct AssetRecord
    {
        AssetGuid guid;
        AssetType type = AssetType::Unknown;
        std::filesystem::path sourcePath;
        std::filesystem::path metaPath;
        std::filesystem::path importedPath;
        std::string importer;
        std::filesystem::file_time_type sourceWriteTime{};
    };

    class AssetDatabase
    {
      public:
        bool Initialize(const std::filesystem::path& assetRoot);
        bool Initialize(const AssetDatabaseCreateInfo& createInfo);
        void Shutdown();

        bool Scan(bool createMissingMeta = true);
        bool RegisterAsset(const std::filesystem::path& sourcePath, bool createMissingMeta = true);

        const AssetRecord* FindByGuid(const AssetGuid& guid) const;
        const AssetRecord* FindByPath(const std::filesystem::path& path) const;
        std::vector<AssetRecord> GetAllAssets() const;
        bool UpdateImportResult(const AssetGuid& guid, const std::string& importer, const std::filesystem::path& importedPath);
        std::vector<AssetGuid> PollChangedAssets();

        const std::filesystem::path& GetAssetRoot() const
        {
            return m_assetRoot;
        }

        static AssetType Classify(const std::filesystem::path& path);
        static std::string AssetTypeName(AssetType type);
        static std::string DefaultImporterName(AssetType type);
        static std::filesystem::path NormalizePath(const std::filesystem::path& path);

      private:
        static AssetGuid GenerateGuid();
        static bool IsIgnored(const std::filesystem::path& path);
        bool LoadMeta(const std::filesystem::path& metaPath, AssetRecord& record) const;
        bool SaveMeta(const AssetRecord& record) const;
        bool IndexRecord(AssetRecord record);

      private:
        std::filesystem::path m_assetRoot;
        std::unordered_map<std::string, AssetRecord> m_recordsByGuid;
        std::unordered_map<std::string, std::string> m_guidByPath;
    };
} // namespace ChikaEngine::Asset
