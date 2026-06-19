#pragma once

#include "ChikaEngine/AssetReference.hpp"
#include "ChikaEngine/RenderSettings.hpp"
#include "ChikaEngine/Window/WinDesc.hpp"
#include "ChikaEngine/project/RuntimeMode.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace ChikaEngine::Project
{
    struct ProjectRuntimeSettings
    {
        Render::RenderPipelineMode renderPipeline = Render::RenderPipelineMode::Forward;
        float fixedDeltaTime = 1.0f / 60.0f;
        uint32_t maxPhysicsStepsPerFrame = 4;
        bool enableScripting = true;
    };

    /**
     * @brief 保存类似轻量 `.uproject` 的项目身份和启动配置。
     *
     * Descriptor 只保存相对项目根路径与稳定 GUID，因此可在机器和工作目录之间移动。
     */
    struct ProjectDescriptor
    {
        uint32_t version = 0;
        std::string name;
        std::filesystem::path descriptorPath;
        std::filesystem::path projectRoot;
        std::filesystem::path contentRoot;
        std::filesystem::path cookedContentRoot;
        Asset::AssetReference startupScene;
        std::vector<Asset::AssetReference> alwaysCook;
        Platform::WindowDesc window;
        ProjectRuntimeSettings runtime;

        /**
         * @brief 加载并完整验证 Project Descriptor。
         *
         * 验证失败不会产生部分可用配置，调用方应直接终止启动并展示 error。
         */
        static bool Load(const std::filesystem::path& path, ProjectDescriptor& descriptor, std::string& error);
    };

    /**
     * @brief 保存 Project Descriptor 投影后的只读 Runtime 启动能力。
     */
    struct RuntimeBootConfig
    {
        RuntimeMode mode = RuntimeMode::DevelopmentGame;
        std::string projectName;
        std::filesystem::path contentRoot;
        Asset::AssetReference startupScene;
        Platform::WindowDesc window;
        ProjectRuntimeSettings runtime;
        bool createContentRoot = false;
        bool scanAssets = true;
        bool createMissingMeta = false;
        bool importAssets = true;
        bool enableHotReload = true;
        bool createDefaultScene = false;
        bool useEditorView = false;
    };

    /**
     * @brief 将已验证 Project 配置投影成明确的 RuntimeMode 能力集合。
     */
    bool BuildRuntimeBootConfig(const ProjectDescriptor& descriptor, RuntimeMode mode, RuntimeBootConfig& config, std::string& error);
} // namespace ChikaEngine::Project
