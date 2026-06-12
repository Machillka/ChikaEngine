#include "ChikaEngine/AssetImporter.hpp"
#include "ChikaEngine/debug/log_macros.h"

namespace ChikaEngine::Asset
{
    ImportResult PassthroughImporter::Import(const AssetRecord& record)
    {
        const bool exists = std::filesystem::is_regular_file(record.sourcePath);
        return {
            .success = exists,
            .importedPath = record.sourcePath,
            .message = exists ? "" : "Source file does not exist",
        };
    }

    ImportResult ShaderImporter::Import(const AssetRecord& record)
    {
        std::filesystem::path output = record.importedPath;
        if (output.empty() || output == record.sourcePath)
            output = record.sourcePath.string() + ".spv";

        std::error_code error;
        const bool outputExists = std::filesystem::is_regular_file(output);
        const bool sourceIsNewer = !outputExists || std::filesystem::last_write_time(record.sourcePath, error) > std::filesystem::last_write_time(output, error);
        if (!sourceIsNewer)
            return { .success = true, .importedPath = output };

        const bool success = m_compiler.Compile(record.sourcePath, output);
        return {
            .success = success,
            .importedPath = output,
            .message = success ? "" : "Shader compilation failed",
        };
    }

    void ImporterRegistry::Register(std::unique_ptr<IAssetImporter> importer)
    {
        if (!importer)
            return;
        const std::string name = importer->Name();
        m_importers[name] = std::move(importer);
    }

    IAssetImporter* ImporterRegistry::Find(const std::string& name) const
    {
        const auto it = m_importers.find(name);
        return it == m_importers.end() ? nullptr : it->second.get();
    }

    ImportResult ImporterRegistry::Import(AssetDatabase& database, const AssetGuid& guid) const
    {
        const AssetRecord* record = database.FindByGuid(guid);
        if (!record)
            return { .message = "Asset GUID is not registered" };

        IAssetImporter* importer = Find(record->importer);
        if (!importer)
            return { .message = "Importer is not registered: " + record->importer };

        ImportResult result = importer->Import(*record);
        if (result.success)
            database.UpdateImportResult(guid, importer->Name(), result.importedPath);
        else
            LOG_ERROR("ImporterRegistry", "Failed to import {}: {}", record->sourcePath.string(), result.message);
        return result;
    }

    size_t ImporterRegistry::ImportAll(AssetDatabase& database) const
    {
        size_t importedCount = 0;
        for (const auto& record : database.GetAllAssets())
        {
            if (Import(database, record.guid).success)
                ++importedCount;
        }
        return importedCount;
    }

    ImporterRegistry ImporterRegistry::CreateDefault()
    {
        ImporterRegistry registry;
        registry.Register(std::make_unique<PassthroughImporter>());
        registry.Register(std::make_unique<ShaderImporter>());
        return registry;
    }
} // namespace ChikaEngine::Asset
