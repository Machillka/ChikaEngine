#include "ChikaEngine/Application.hpp"
#include "ChikaEngine/project/ProjectDescriptor.hpp"
#include "ChikaEngine/scene/SceneManager.hpp"
#include "ChikaEngine/scene/scene.hpp"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/debug/log_system.h"

#include <charconv>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ChikaEngine::Game
{
    /**
     * @brief Standalone 游戏进程入口，只复用 Runtime Application 生命周期。
     *
     * 该入口不创建 Editor、Panel 或 UI Backend；`--smoke-frames` 仅用于自动验证
     * 初始化、主循环和逆序关闭路径，正式运行时保持为零并由窗口关闭事件退出。
     */
    class GameApplication final : public Engine::Application
    {
      public:
        GameApplication(Project::RuntimeBootConfig bootConfig, uint32_t smokeFrameCount) : m_bootConfig(std::move(bootConfig)), m_smokeFrameCount(smokeFrameCount) {}

      protected:
        /**
         * @brief 提供 Standalone 窗口配置，后续 Project Descriptor 将覆盖这里的临时默认值。
         */
        Engine::EngineContextCreateInfo CreateEngineContextInfo() const override
        {
            Engine::EngineContextCreateInfo createInfo;
            createInfo.window = m_bootConfig.window;
            createInfo.runtimeMode = m_bootConfig.mode;
            createInfo.contentRoot = m_bootConfig.contentRoot;
            createInfo.createContentRoot = m_bootConfig.createContentRoot;
            createInfo.scanAssets = m_bootConfig.scanAssets;
            createInfo.createMissingMeta = m_bootConfig.createMissingMeta;
            createInfo.importAssets = m_bootConfig.importAssets;
            createInfo.enableHotReload = m_bootConfig.enableHotReload;
            createInfo.createDefaultScene = m_bootConfig.createDefaultScene;
            createInfo.useEditorView = m_bootConfig.useEditorView;
            createInfo.renderPipeline = m_bootConfig.runtime.renderPipeline;
            createInfo.fixedDeltaTime = m_bootConfig.runtime.fixedDeltaTime;
            createInfo.maxPhysicsStepsPerFrame = m_bootConfig.runtime.maxPhysicsStepsPerFrame;
            createInfo.enableScripting = m_bootConfig.runtime.enableScripting;
            return createInfo;
        }

        /**
         * @brief 按 Project 启动 Scene GUID 加载、激活并进入 Play。
         *
         * Game Runtime 不创建 Editor Baseline Scene，也不允许缺失 Primary Camera 静默启动。
         */
        void OnInitialize() override
        {
            auto& context = GetEngineContext();
            Framework::Scene* scene = context.GetSceneManager()->LoadScene(m_bootConfig.startupScene.GetGuid(), true);
            if (!scene)
                throw std::runtime_error("Failed to load startup Scene GUID: " + m_bootConfig.startupScene.guid);
            if (!scene->ValidateRuntimeAssetReferences())
                throw std::runtime_error("Startup Scene contains missing or invalid direct asset references");
            if (!scene->HasPrimaryCamera())
                throw std::runtime_error("Startup Scene does not contain an active Primary Camera");
            if (!scene->StartPlayMode())
                throw std::runtime_error("Startup Scene failed to enter Play mode");
        }

        /**
         * @brief 在 Smoke 模式达到指定帧数后请求退出，验证正常关闭而非强制终止。
         */
        void OnUpdate(float) override
        {
            if (m_smokeFrameCount > 0 && ++m_frameCount >= m_smokeFrameCount)
                RequestExit();
        }

      private:
        Project::RuntimeBootConfig m_bootConfig;
        uint32_t m_smokeFrameCount = 0;
        uint32_t m_frameCount = 0;
    };

    /**
     * @brief 解析可选的 `--smoke-frames N`，无效输入回退为普通交互运行。
     */
    uint32_t ParseSmokeFrameCount(int argc, char** argv)
    {
        for (int index = 1; index + 1 < argc; ++index)
        {
            if (std::string_view(argv[index]) != "--smoke-frames")
                continue;

            uint32_t value = 0;
            const std::string_view text(argv[index + 1]);
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec == std::errc{} && result.ptr == text.data() + text.size())
                return value;
        }
        return 0;
    }

    /** @brief 读取 `--project`，未指定时使用仓库根目录示例项目。 */
    std::filesystem::path ParseProjectPath(int argc, char** argv)
    {
        for (int index = 1; index + 1 < argc; ++index)
        {
            if (std::string_view(argv[index]) == "--project")
                return argv[index + 1];
        }
        return "ChikaProject.json";
    }

    /** @brief 读取 `--mode development|packaged`，Game 默认使用 Development Runtime。 */
    Project::RuntimeMode ParseRuntimeMode(int argc, char** argv)
    {
        for (int index = 1; index + 1 < argc; ++index)
        {
            if (std::string_view(argv[index]) != "--mode")
                continue;
            if (std::string_view(argv[index + 1]) == "packaged")
                return Project::RuntimeMode::PackagedGame;
        }
        return Project::RuntimeMode::DevelopmentGame;
    }
} // namespace ChikaEngine::Game

int main(int argc, char** argv)
{
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());
    ChikaEngine::Project::ProjectDescriptor descriptor;
    std::string error;
    if (!ChikaEngine::Project::ProjectDescriptor::Load(ChikaEngine::Game::ParseProjectPath(argc, argv), descriptor, error))
    {
        LOG_ERROR("ChikaGame", "Project load failed: {}", error);
        return 2;
    }

    ChikaEngine::Project::RuntimeBootConfig bootConfig;
    if (!ChikaEngine::Project::BuildRuntimeBootConfig(descriptor, ChikaEngine::Game::ParseRuntimeMode(argc, argv), bootConfig, error))
    {
        LOG_ERROR("ChikaGame", "Runtime boot config failed: {}", error);
        return 3;
    }

    ChikaEngine::Game::GameApplication application(std::move(bootConfig), ChikaEngine::Game::ParseSmokeFrameCount(argc, argv));
    return application.Run();
}
