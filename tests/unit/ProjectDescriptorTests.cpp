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
    if (!WriteText(descriptorPath, R"({"version":1,"name":"Sample","contentRoot":"Assets","cookedContentRoot":"Content","startupScene":"70a1e96c29ca4c9ab61d65d9f127c143","alwaysCook":[],"window":{"title":"Sample","width":800,"height":600,"fullscreen":false,"vSync":false},"runtime":{"renderPipeline":"deferred","fixedDeltaTime":0.02,"maxPhysicsStepsPerFrame":3,"enableScripting":false}})"))
        return Fail("failed to write valid descriptor");

    Project::ProjectDescriptor descriptor;
    std::string error;
    if (!Project::ProjectDescriptor::Load(descriptorPath, descriptor, error))
        return Fail("valid descriptor was rejected");

    Project::RuntimeBootConfig development;
    if (!Project::BuildRuntimeBootConfig(descriptor, Project::RuntimeMode::DevelopmentGame, development, error) || !development.scanAssets || development.createMissingMeta || !development.enableHotReload || development.window.vSync || development.contentRoot != (root / "Assets").lexically_normal())
        return Fail("development boot config projection is incorrect");

    Project::RuntimeBootConfig packaged;
    if (!Project::BuildRuntimeBootConfig(descriptor, Project::RuntimeMode::PackagedGame, packaged, error) || packaged.scanAssets || packaged.importAssets || packaged.enableHotReload || packaged.contentRoot != (root / "Content").lexically_normal())
        return Fail("packaged boot config projection is incorrect");

    if (!WriteText(descriptorPath, R"({"version":2,"name":"Invalid","contentRoot":"../Assets","cookedContentRoot":"Content","startupScene":""})") || Project::ProjectDescriptor::Load(descriptorPath, descriptor, error))
        return Fail("invalid descriptor was accepted");

    std::filesystem::remove_all(root, filesystemError);
    return 0;
}
