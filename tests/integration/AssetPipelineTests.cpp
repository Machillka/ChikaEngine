#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/AssetImporter.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/AssetReference.hpp"
#include "ChikaEngine/MaterialLoader.hpp"
#include "ChikaEngine/ShaderReflection.hpp"
#include "ChikaEngine/ShaderTemplateLoader.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
    bool WriteBytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return file.good();
    }

    bool WriteText(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::trunc);
        file << text;
        return file.good();
    }
} // namespace

int main()
{
    namespace Asset = ChikaEngine::Asset;

    const auto root = std::filesystem::temp_directory_path() / "chika_asset_pipeline_tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    if (error)
        return 1;

    const auto source = root / "identity.asset";
    const auto generatedShaderSource = root / "generated.vert";
    const auto generatedShaderBinary = root / "generated.vert.spv";
    const auto compiledShaderSource = root / "compiled.vert";
    const auto sceneSource = root / "startup.scene";
    if (!WriteBytes(source, { 1, 2, 3 }) || !WriteBytes(generatedShaderSource, { 1 }) || !WriteBytes(generatedShaderBinary, { 1, 2, 3, 4 }) || !WriteText(compiledShaderSource, "#version 450\nvoid main() { gl_Position = vec4(0.0); }\n") || !WriteText(sceneSource, R"({"Scene":{"GameObjects":[]}})"))
        return 2;

    Asset::AssetDatabase database;
    if (!database.Initialize(root))
        return 3;
    const Asset::AssetRecord* firstRecord = database.FindByPath(source);
    if (!firstRecord || !firstRecord->guid.IsValid() || !std::filesystem::exists(source.string() + ".meta"))
        return 4;
    if (!database.FindByPath(generatedShaderSource) || database.FindByPath(generatedShaderBinary))
        return 15;
    const Asset::AssetRecord* sceneRecord = database.FindByPath(sceneSource);
    if (!sceneRecord || sceneRecord->type != Asset::AssetType::Scene || sceneRecord->importer != "scene")
        return 18;
    const std::string stableGuid = firstRecord->guid.value;

    database.Shutdown();
    if (!database.Initialize(root))
        return 5;
    const Asset::AssetRecord* secondRecord = database.FindByPath(source);
    if (!secondRecord || secondRecord->guid.value != stableGuid)
        return 6;

    const auto movedSource = root / "moved.asset";
    std::filesystem::rename(source, movedSource, error);
    std::filesystem::rename(source.string() + ".meta", movedSource.string() + ".meta", error);
    if (error || !database.Scan())
        return 19;
    const Asset::AssetRecord* movedRecord = database.FindByGuid({ stableGuid });
    if (!movedRecord || movedRecord->sourcePath != Asset::AssetDatabase::NormalizePath(movedSource))
        return 20;

    const auto duplicateSource = root / "duplicate.asset";
    if (!WriteBytes(duplicateSource, { 4, 5, 6 }) || !WriteText(duplicateSource.string() + ".meta", R"({"version":1,"guid":")" + stableGuid + R"(","type":"unknown","importer":"passthrough","imported":"duplicate.asset"})"))
        return 21;
    if (database.Scan())
        return 22;
    std::filesystem::remove(duplicateSource, error);
    std::filesystem::remove(duplicateSource.string() + ".meta", error);
    if (!database.Scan())
        return 23;

    auto importers = Asset::ImporterRegistry::CreateDefault();
    movedRecord = database.FindByGuid({ stableGuid });
    if (!movedRecord || !importers.Import(database, movedRecord->guid).success)
        return 7;
    const Asset::AssetRecord* compiledShaderRecord = database.FindByPath(compiledShaderSource);
    if (!compiledShaderRecord || !importers.Import(database, compiledShaderRecord->guid).success || !std::filesystem::exists(compiledShaderSource.string() + ".spv") || !std::filesystem::exists(Asset::ShaderReflection::SidecarPath(compiledShaderSource.string() + ".spv")))
        return 16;

    const auto shaderPath = root / "runtime.spv";
    if (!WriteBytes(shaderPath, { 1, 2, 3, 4 }))
        return 8;

    Asset::AssetManager assets;
    if (!assets.Initialize(root, false))
        return 9;

    const Asset::AssetReference movedReference{ Asset::AssetGuid{ stableGuid }, Asset::AssetType::Unknown };
    if (!assets.ResolveReference(movedReference, Asset::AssetType::Unknown, "moved test asset"))
        return 24;

    const Asset::ShaderHandle compiledShader = assets.LoadShader(compiledShaderSource.string());
    const Asset::ShaderData* compiledData = assets.GetShader(compiledShader);
    if (!compiledData || !compiledData->hasReflection || compiledData->reflection.stage != ChikaEngine::Shader::ShaderStageMask::Vertex)
        return 17;

    const Asset::ShaderHandle shader = assets.LoadShaderAsync(shaderPath.string()).get();
    const Asset::AssetRecord* shaderRecord = assets.GetDatabase().FindByPath(shaderPath);
    const Asset::ShaderData* initialData = assets.GetShader(shader);
    if (!shader.IsValid() || !shaderRecord || assets.LoadShader(shaderRecord->guid) != shader || !initialData || initialData->spirv.size() != 4)
        return 10;

    size_t reloadEvents = 0;
    assets.SubscribeReload(
        [&](const Asset::AssetReloadEvent& event)
        {
            if (event.sourcePath == Asset::AssetDatabase::NormalizePath(shaderPath))
                ++reloadEvents;
        });

    if (!WriteBytes(shaderPath, { 1, 2, 3, 4, 5, 6, 7, 8 }))
        return 11;
    std::filesystem::last_write_time(shaderPath, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(2), error);
    if (error || assets.TickHotReload() != 1)
        return 12;

    const Asset::ShaderData* reloadedData = assets.GetShader(shader);
    if (!reloadedData || reloadedData->spirv.size() != 8 || reloadEvents != 1)
        return 13;

    if (!assets.Unload(shader) || assets.GetShader(shader) != nullptr)
        return 14;

    const auto material = Asset::MaterialLoader::Load("Assets/Materials/floor.json");
    const auto shaderTemplate = Asset::ShaderTemplateLoader::Load("Assets/Materials/base.template.json");
    if (!material || !material->shaderTemplate.IsValid() || !material->textureParams.at("Albedo").IsValid() || !shaderTemplate || !shaderTemplate->vertexShader.IsValid() || !shaderTemplate->fragmentShader.IsValid())
        return 25;

    assets.Shutdown();
    std::filesystem::remove_all(root, error);
    std::cout << "Asset pipeline integration checks passed\n";
    return 0;
}
