#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/jobs/JobSystem.hpp"
#include "ChikaEngine/profiler/ProfilerName.hpp"
#include "ChikaEngine/profiler/ProfilerSession.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
    /** @brief Writes a minimal binary payload accepted by the SPIR-V byte loader for scheduler tests. */
    bool WriteShader(const std::filesystem::path& path, uint32_t marker)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(&marker), sizeof(marker));
        return stream.good();
    }
} // namespace

int main()
{
    namespace Asset = ChikaEngine::Asset;
    namespace Jobs = ChikaEngine::Jobs;
    namespace Profiler = ChikaEngine::Profiler;

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "chika_asset_job_tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    if (error)
        return 1;

    std::vector<std::filesystem::path> shaderPaths;
    for (uint32_t index = 0; index < 8; ++index)
    {
        const auto path = root / ("shader-" + std::to_string(index) + ".spv");
        if (!WriteShader(path, index + 1u))
            return 2;
        shaderPaths.push_back(path);
    }

    Jobs::JobSystem jobs;
    if (!jobs.Initialize({ .workerCount = 4, .jobCapacity = 1'024 }))
        return 3;
    Asset::AssetManager assets;
    if (!assets.Initialize({ .assetRoot = root, .scanAssets = false, .createMissingMeta = false, .importAssets = false, .enableHotReload = false, .jobSystem = &jobs }))
        return 4;

    Profiler::ProfilerSession& profiler = Profiler::ProfilerSession::Get();
    profiler.Initialize({ .enabled = true, .historyCapacity = 4 });
    profiler.BeginFrame(300);

    std::vector<std::shared_future<Asset::ShaderHandle>> duplicates;
    for (uint32_t index = 0; index < 32; ++index)
        duplicates.push_back(assets.LoadShaderAsync(shaderPaths.front().string()));
    const Asset::ShaderHandle duplicateHandle = duplicates.front().get();
    for (auto& request : duplicates)
    {
        if (request.get() != duplicateHandle)
            return 5;
    }
    if (jobs.GetStatistics().submittedJobs != 1)
        return 6;

    if (!assets.Unload(duplicateHandle))
        return 10;
    const Asset::ShaderHandle reloadedHandle = assets.LoadShaderAsync(shaderPaths.front().string()).get();
    if (!reloadedHandle.IsValid() || reloadedHandle == duplicateHandle)
        return 11;

    std::vector<std::shared_future<Asset::ShaderHandle>> requests;
    for (size_t index = 1; index < shaderPaths.size(); ++index)
        requests.push_back(assets.LoadShaderAsync(shaderPaths[index].string()));
    for (auto& request : requests)
    {
        if (!request.get().IsValid())
            return 7;
    }

    bool missingFailed = false;
    try
    {
        assets.LoadShaderAsync((root / "missing.spv").string()).get();
    }
    catch (const std::runtime_error&)
    {
        missingFailed = true;
    }
    if (!missingFailed)
        return 8;
    const auto missingPath = root / "missing.spv";
    if (!WriteShader(missingPath, 42) || !assets.LoadShaderAsync(missingPath.string()).get().IsValid())
        return 12;

    profiler.EndFrame(300);
    const auto capture = profiler.GetHistory().Find(300);
    bool foundAssetWorkerZone = false;
    if (capture)
    {
        for (const auto& zone : capture->zones)
            foundAssetWorkerZone = foundAssetWorkerZone || Profiler::ProfilerNameRegistry::Instance().Resolve(zone.nameId) == "Asset.LoadShader";
    }
    if (!foundAssetWorkerZone)
        return 9;

    assets.Shutdown();
    jobs.Shutdown(Jobs::JobShutdownPolicy::Drain);
    profiler.Shutdown();
    std::filesystem::remove_all(root, error);
    std::cout << "Asset job integration checks passed\n";
    return 0;
}
