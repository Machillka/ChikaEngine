#include "ChikaEngine/AssetImporter.hpp"
#include "ChikaEngine/ShaderReflection.hpp"
#include "ChikaEngine/debug/log_macros.h"

#include <fstream>
#include <optional>

namespace ChikaEngine::Asset
{
    namespace
    {
        /**
         * @brief 校验当前引擎 Descriptor Set 频率约定。
         *
         * set 0 保存 frame/scene，set 1 保存 material，set 2 保存 object；Importer 在运行前阻止常见错放。
         */
        bool ValidateEngineSetConvention(const Shader::ShaderReflectionData& reflection, std::string& error)
        {
            for (const auto& resource : reflection.resources)
            {
                if (resource.set > 2)
                {
                    error = "Descriptor '" + resource.name + "' uses unsupported set " + std::to_string(resource.set);
                    return false;
                }

                std::optional<uint32_t> expectedSet;
                if (resource.name == "scene" || resource.name == "shadowMap" || resource.name.starts_with("GBuffer"))
                    expectedSet = 0;
                else if (resource.name == "material" || resource.name == "Albedo")
                    expectedSet = 1;
                else if (resource.name == "uboBones" || resource.name == "instances")
                    expectedSet = 2;

                if (expectedSet && resource.set != *expectedSet)
                {
                    error = "Descriptor '" + resource.name + "' must use engine frequency set " + std::to_string(*expectedSet);
                    return false;
                }
            }
            return true;
        }

        /**
         * @brief 读取刚生成的 SPIR-V，并持久化同一逻辑 Shader 的 Reflection 导入产物。
         *
         * Reflection Sidecar 不注册 GUID；它与 SPIR-V 一起由源 Shader 的 meta 管理。
         */
        bool ReflectImportedShader(const std::filesystem::path& output, std::string& error)
        {
            std::ifstream file(output, std::ios::binary);
            const std::vector<uint8_t> spirv(std::istreambuf_iterator<char>(file), {});
            if (spirv.empty())
            {
                error = "Compiled shader is empty";
                return false;
            }

            Shader::ShaderReflectionData reflection;
            if (!ShaderReflection::Reflect(spirv, reflection, error))
                return false;
            if (!ValidateEngineSetConvention(reflection, error))
                return false;
            return ShaderReflection::Save(ShaderReflection::SidecarPath(output), reflection, error);
        }
    } // namespace

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
        const std::filesystem::path reflectionPath = ShaderReflection::SidecarPath(output);
        const bool reflectionExists = std::filesystem::is_regular_file(reflectionPath);
        const bool sourceIsNewer = !outputExists || !reflectionExists || std::filesystem::last_write_time(record.sourcePath, error) > std::filesystem::last_write_time(output, error);
        if (!sourceIsNewer && reflectionExists)
        {
            Shader::ShaderReflectionData existingReflection;
            std::string reflectionError;
            if (ShaderReflection::Load(reflectionPath, existingReflection, reflectionError))
            {
                if (ValidateEngineSetConvention(existingReflection, reflectionError))
                    return { .success = true, .importedPath = output };
                return { .importedPath = output, .message = reflectionError };
            }
            if (ReflectImportedShader(output, reflectionError))
                return { .success = true, .importedPath = output };
            return { .importedPath = output, .message = reflectionError };
        }

        const bool success = m_compiler.Compile(record.sourcePath, output);
        std::string reflectionError;
        const bool reflectionSuccess = success && ReflectImportedShader(output, reflectionError);
        return {
            .success = reflectionSuccess,
            .importedPath = output,
            .message = !success ? "Shader compilation failed" : reflectionError,
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
