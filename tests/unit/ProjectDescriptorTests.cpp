#include "ChikaEngine/project/ProjectDescriptor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
    bool WriteText(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::trunc);
        file << text;
        return file.good();
    }

    int Fail(const char* message)
    {
        std::cerr << "FAILED: " << message << '\n';
        return 1;
    }
} // namespace

int main()
{
    namespace Project = ChikaEngine::Project;

    const auto root = std::filesystem::temp_directory_path() / "chika_project_descriptor_tests";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root / "Assets", filesystemError);
    std::filesystem::create_directories(root / "Content", filesystemError);
    const auto descriptorPath = root / "Sample.chikaproject";
    if (!WriteText(
            descriptorPath,
            R"({"version":1,"name":"Sample","contentRoot":"Assets","cookedContentRoot":"Content","startupScene":"70a1e96c29ca4c9ab61d65d9f127c143","alwaysCook":[],"window":{"title":"Sample","width":800,"height":600,"fullscreen":false,"vSync":false},"runtime":{"renderPipeline":"deferred","environment":{"enabled":true,"skybox":{"guid":"67d279940ad24613a5be745bec80fdb2","path":"Assets/Textures/Skybox/default-skybox.texture"},"intensity":1.5,"useFallback":false,"fallbackColor":[0.1,0.2,0.3,1.0]},"fixedDeltaTime":0.02,"maxPhysicsStepsPerFrame":3,"enableScripting":false}})"))
        return Fail("failed to write valid descriptor");

    Project::ProjectDescriptor descriptor;
    std::string error;
    if (!Project::ProjectDescriptor::Load(descriptorPath, descriptor, error))
        return Fail("valid descriptor was rejected");
    if (!descriptor.runtime.environment.enabled || descriptor.runtime.environment.useFallback || descriptor.runtime.environment.intensity != 1.5f || !descriptor.runtime.environment.skybox.IsValid() || descriptor.runtime.environment.skybox.GetExpectedType() != ChikaEngine::Asset::AssetType::Texture)
        return Fail("environment settings were not parsed");

    Project::RuntimeBootConfig development;
    if (!Project::BuildRuntimeBootConfig(descriptor, Project::RuntimeMode::DevelopmentGame, development, error) || !development.scanAssets || development.createMissingMeta || !development.enableHotReload || development.window.vSync || development.contentRoot != (root / "Assets").lexically_normal() || !development.runtime.environment.enabled || development.runtime.environment.skybox.guid != descriptor.runtime.environment.skybox.guid)
        return Fail("development boot config projection is incorrect");

    Project::RuntimeBootConfig packaged;
    if (!Project::BuildRuntimeBootConfig(descriptor, Project::RuntimeMode::PackagedGame, packaged, error) || packaged.scanAssets || packaged.importAssets || packaged.enableHotReload || packaged.contentRoot != (root / "Content").lexically_normal())
        return Fail("packaged boot config projection is incorrect");

    if (!WriteText(descriptorPath, R"({"version":2,"name":"Invalid","contentRoot":"../Assets","cookedContentRoot":"Content","startupScene":""})") || Project::ProjectDescriptor::Load(descriptorPath, descriptor, error))
        return Fail("invalid descriptor was accepted");

    if (!WriteText(descriptorPath, R"({"version":1,"name":"Zero Width","contentRoot":"Assets","cookedContentRoot":"Content","startupScene":"70a1e96c29ca4c9ab61d65d9f127c143","window":{"width":0,"height":600}})") || Project::ProjectDescriptor::Load(descriptorPath, descriptor, error))
        return Fail("zero-width window was accepted");

    if (!WriteText(descriptorPath, R"({"version":1,"name":"Zero Height","contentRoot":"Assets","cookedContentRoot":"Content","startupScene":"70a1e96c29ca4c9ab61d65d9f127c143","window":{"width":800,"height":0}})") || Project::ProjectDescriptor::Load(descriptorPath, descriptor, error))
        return Fail("zero-height window was accepted");

    if (!WriteText(descriptorPath, R"({"version":1,"name":"Invalid Environment","contentRoot":"Assets","cookedContentRoot":"Content","startupScene":"70a1e96c29ca4c9ab61d65d9f127c143","runtime":{"environment":{"enabled":true,"useFallback":false,"intensity":-1.0}}})") || Project::ProjectDescriptor::Load(descriptorPath, descriptor, error))
        return Fail("invalid environment settings were accepted");

    Project::ProjectDescriptor repositoryProject;
    const bool loadedRepositoryProject = Project::ProjectDescriptor::Load("ChikaProject.json", repositoryProject, error);
    if (!loadedRepositoryProject || repositoryProject.runtime.environment.skybox.guid != "4e5a4cb0d97f4e9d91f2d7bb0bd1ad12" || repositoryProject.runtime.environment.skybox.diagnosticPath != "Assets/Textures/Skybox/NightSkyHDRI008_2K_HDR.texture")
    {
        return Fail("repository NightSky EXR environment project configuration is invalid");
    }

    std::filesystem::remove_all(root, filesystemError);
    return 0;
}
