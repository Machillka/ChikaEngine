#include "ChikaEngine/project/ProjectDescriptor.hpp"

#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

namespace ChikaEngine::Project
{
    namespace
    {
        bool IsSafeRelativePath(const std::filesystem::path& path)
        {
            if (path.empty() || path.is_absolute())
                return false;
            for (const auto& part : path)
            {
                if (part == "..")
                    return false;
            }
            return true;
        }

        Asset::AssetReference ParseReference(const nlohmann::json& value, Asset::AssetType expectedType)
        {
            if (value.is_string())
                return { Asset::AssetGuid{ value.get<std::string>() }, expectedType };
            return {
                Asset::AssetGuid{ value.value("guid", "") },
                expectedType,
                value.value("subAsset", ""),
                value.value("path", ""),
            };
        }

        bool ParsePipeline(const std::string& name, Render::RenderPipelineMode& pipeline)
        {
            if (name == "forward")
            {
                pipeline = Render::RenderPipelineMode::Forward;
                return true;
            }
            if (name == "deferred")
            {
                pipeline = Render::RenderPipelineMode::Deferred;
                return true;
            }
            return false;
        }

        bool ParseEnvironment(const nlohmann::json& json, Render::EnvironmentSettings& environment, std::string& error)
        {
            if (!json.is_object())
            {
                error = "runtime.environment must be an object";
                return false;
            }

            Render::EnvironmentSettings parsed;
            parsed.enabled = json.value("enabled", parsed.enabled);
            parsed.useFallback = json.value("useFallback", parsed.useFallback);
            parsed.intensity = json.value("intensity", parsed.intensity);
            if (json.contains("skybox"))
                parsed.skybox = ParseReference(json["skybox"], Asset::AssetType::Texture);

            if (json.contains("fallbackColor"))
            {
                const nlohmann::json& color = json["fallbackColor"];
                if (!color.is_array() || color.size() != parsed.fallbackColor.size())
                {
                    error = "runtime.environment.fallbackColor must contain exactly 4 numbers";
                    return false;
                }
                for (size_t index = 0; index < parsed.fallbackColor.size(); ++index)
                {
                    if (!color[index].is_number())
                    {
                        error = "runtime.environment.fallbackColor must contain exactly 4 numbers";
                        return false;
                    }
                    parsed.fallbackColor[index] = color[index].get<float>();
                }
            }

            if (!std::isfinite(parsed.intensity) || parsed.intensity < 0.0f)
            {
                error = "runtime.environment.intensity must be finite and non-negative";
                return false;
            }
            for (float component : parsed.fallbackColor)
            {
                if (!std::isfinite(component))
                {
                    error = "runtime.environment.fallbackColor components must be finite";
                    return false;
                }
            }
            if (parsed.enabled && !parsed.skybox.IsValid() && parsed.skybox.diagnosticPath.empty() && !parsed.useFallback)
            {
                error = "runtime.environment requires a skybox reference when fallback is disabled";
                return false;
            }

            environment = std::move(parsed);
            return true;
        }
    } // namespace

    bool ProjectDescriptor::Load(const std::filesystem::path& path, ProjectDescriptor& descriptor, std::string& error)
    {
        try
        {
            std::ifstream file(path);
            if (!file.is_open())
            {
                error = "Project descriptor does not exist: " + path.string();
                return false;
            }

            const nlohmann::json json = nlohmann::json::parse(file);
            ProjectDescriptor loaded;
            loaded.descriptorPath = std::filesystem::absolute(path).lexically_normal();
            loaded.projectRoot = loaded.descriptorPath.parent_path();
            loaded.version = json.value("version", 0u);
            loaded.name = json.value("name", "");
            loaded.contentRoot = json.value("contentRoot", "");
            loaded.cookedContentRoot = json.value("cookedContentRoot", "");
            if (json.contains("startupScene"))
                loaded.startupScene = ParseReference(json["startupScene"], Asset::AssetType::Scene);
            if (json.contains("alwaysCook"))
            {
                for (const auto& reference : json["alwaysCook"])
                    loaded.alwaysCook.push_back(ParseReference(reference, Asset::AssetType::Unknown));
            }

            if (json.contains("window"))
            {
                const auto& window = json["window"];
                loaded.window.title = window.value("title", loaded.name);
                loaded.window.width = window.value("width", loaded.window.width);
                loaded.window.height = window.value("height", loaded.window.height);
                loaded.window.isFullscreen = window.value("fullscreen", loaded.window.isFullscreen);
                loaded.window.vSync = window.value("vSync", loaded.window.vSync);
            }
            if (json.contains("runtime"))
            {
                const auto& runtime = json["runtime"];
                if (!ParsePipeline(runtime.value("renderPipeline", "forward"), loaded.runtime.renderPipeline))
                {
                    error = "Unsupported renderPipeline";
                    return false;
                }
                loaded.runtime.fixedDeltaTime = runtime.value("fixedDeltaTime", loaded.runtime.fixedDeltaTime);
                loaded.runtime.maxPhysicsStepsPerFrame = runtime.value("maxPhysicsStepsPerFrame", loaded.runtime.maxPhysicsStepsPerFrame);
                loaded.runtime.enableScripting = runtime.value("enableScripting", loaded.runtime.enableScripting);
                if (runtime.contains("environment") && !ParseEnvironment(runtime["environment"], loaded.runtime.environment, error))
                    return false;
            }

            if (loaded.version != 1)
                error = "Unsupported or missing project descriptor version";
            else if (loaded.name.empty())
                error = "Project name is required";
            else if (!IsSafeRelativePath(loaded.contentRoot))
                error = "contentRoot must be a safe relative path";
            else if (!IsSafeRelativePath(loaded.cookedContentRoot))
                error = "cookedContentRoot must be a safe relative path";
            else if (!loaded.startupScene.IsValid())
                error = "startupScene must contain a valid GUID";
            else if (loaded.runtime.fixedDeltaTime <= 0.0f || loaded.runtime.maxPhysicsStepsPerFrame == 0)
                error = "Runtime physics settings are invalid";
            else
            {
                descriptor = std::move(loaded);
                error.clear();
                return true;
            }
            return false;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
    }

    bool BuildRuntimeBootConfig(const ProjectDescriptor& descriptor, RuntimeMode mode, RuntimeBootConfig& config, std::string& error)
    {
        if (descriptor.version != 1 || descriptor.projectRoot.empty() || !descriptor.startupScene.IsValid())
        {
            error = "Project descriptor must be loaded and validated before building boot config";
            return false;
        }

        if (mode == RuntimeMode::PackagedGame && descriptor.contentRoot.lexically_normal() == descriptor.cookedContentRoot.lexically_normal())
        {
            error = "Packaged Game cookedContentRoot must not point at the source contentRoot";
            return false;
        }

        RuntimeBootConfig built;
        built.mode = mode;
        built.projectName = descriptor.name;
        built.startupScene = descriptor.startupScene;
        built.window = descriptor.window;
        built.runtime = descriptor.runtime;
        built.contentRoot = (descriptor.projectRoot / (mode == RuntimeMode::PackagedGame ? descriptor.cookedContentRoot : descriptor.contentRoot)).lexically_normal();

        if (mode == RuntimeMode::Editor)
        {
            built.createContentRoot = true;
            built.createMissingMeta = true;
            built.createDefaultScene = true;
            built.useEditorView = true;
        }
        else if (mode == RuntimeMode::DevelopmentGame)
        {
            built.createContentRoot = false;
            built.createMissingMeta = false;
        }
        else
        {
            built.createContentRoot = false;
            built.scanAssets = false;
            built.createMissingMeta = false;
            built.importAssets = false;
            built.enableHotReload = false;
        }

        config = std::move(built);
        error.clear();
        return true;
    }
} // namespace ChikaEngine::Project
