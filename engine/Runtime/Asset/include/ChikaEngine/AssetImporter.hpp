#pragma once

#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/ShaderCompiler.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace ChikaEngine::Asset
{
    // 重新封装返回结构, 鉴定为不如 Rust 包装方便（
    struct ImportResult
    {
        bool success = false;
        std::filesystem::path importedPath;
        std::string message;
    };

    class IAssetImporter
    {
      public:
        virtual ~IAssetImporter() = default;

        virtual std::string Name() const = 0;
        virtual ImportResult Import(const AssetRecord& record) = 0;
    };

    class PassthroughImporter final : public IAssetImporter
    {
      public:
        std::string Name() const override
        {
            return "passthrough";
        }
        ImportResult Import(const AssetRecord& record) override;
    };

    class ShaderImporter final : public IAssetImporter
    {
      public:
        ShaderImporter() = default;
        explicit ShaderImporter(ShaderCompiler compiler) : m_compiler(std::move(compiler)) {}

        std::string Name() const override
        {
            return "shader";
        }
        ImportResult Import(const AssetRecord& record) override;

      private:
        ShaderCompiler m_compiler;
    };

    class ImporterRegistry
    {
      public:
        void Register(std::unique_ptr<IAssetImporter> importer);
        IAssetImporter* Find(const std::string& name) const;

        ImportResult Import(AssetDatabase& database, const AssetGuid& guid) const;
        size_t ImportAll(AssetDatabase& database) const;

        static ImporterRegistry CreateDefault();

      private:
        std::unordered_map<std::string, std::unique_ptr<IAssetImporter>> m_importers;
    };
} // namespace ChikaEngine::Asset
