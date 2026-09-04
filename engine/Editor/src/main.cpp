#include "ChikaEngine/Application.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/Window/IWindow.hpp"
#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/LightComponent.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/debug/log_system.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/project/ProjectDescriptor.hpp"
#include "ChikaEngine/scene/scene.hpp"
#include "EditorManager.hpp"
#include <memory>
#include <string>
#include <string_view>

namespace ChikaEngine::Editor
{
    namespace
    {
        /**
         * @brief 创建 Renderer 升级期间使用的固定可视基准场景。
         *
         * 场景覆盖静态 Mesh 和蒙皮动画；真正的单对象多材质与透明材质
         * 当前尚无运行时支持，因此作为基线能力缺口记录在 Phase 0 文档中，而不伪造错误效果。
         */
        void CreateRenderBaselineScene(Framework::Scene& scene)
        {
            const auto animatedObjectId = scene.CreateGameobject("Baseline.Skinned.Fox");
            auto* animatedObject = scene.GetGameObject(animatedObjectId);
            animatedObject->AddComponent<Framework::MeshRenderer>("Assets/Meshes/Fox.gltf", "Assets/Materials/fox.json");
            animatedObject->transform->Scale(0.02f);
            animatedObject->transform->Translate(Math::Vector3(0.0f, 0.2f, 0.0f));
            animatedObject->transform->Rotate(Math::Vector3(0.0f, 0.5f, 0.0f));
            animatedObject->AddComponent<Framework::Animator>("Assets/Meshes/Fox.gltf");
            animatedObject->AddComponent<Framework::Rigidbody>();

            const auto planeId = scene.CreateGameobject("Baseline.Static.Floor");
            auto* plane = scene.GetGameObject(planeId);
            plane->AddComponent<Framework::MeshRenderer>("Assets/Meshes/Box.gltf", "Assets/Materials/floor.json");
            plane->transform->Scale(10.0f, 0.1f, 10.0f);

            /**
             * 相同 Mesh/Material 的重复对象用于验证 Phase 3 Batch 与 GPU Instancing。
             * 这些对象应与 Floor 合并为共享状态 Batch，而不是按 GameObject 各自产生 Draw。
             */
            for (int index = 0; index < 4; ++index)
            {
                const auto instanceId = scene.CreateGameobject("Baseline.Instance." + std::to_string(index));
                auto* instance = scene.GetGameObject(instanceId);
                instance->AddComponent<Framework::MeshRenderer>("Assets/Meshes/Box.gltf", "Assets/Materials/floor.json");
                instance->transform->Translate(Math::Vector3(-3.0f + static_cast<float>(index) * 2.0f, 1.0f, 0.0f));
                instance->transform->Scale(0.5f);
                // FIXME: 目前的 Renderer 仍然在每个 GameObject 上单独创建 Draw，未能正确合并为共享状态 Batch。
            }

            // 明确位于主视锥外，用于验证 Visibility 阶段在 Queue 构建前完成剔除。
            const auto culledId = scene.CreateGameobject("Baseline.Culled.Box");
            auto* culled = scene.GetGameObject(culledId);
            culled->AddComponent<Framework::MeshRenderer>("Assets/Meshes/Box.gltf", "Assets/Materials/floor.json");
            culled->transform->Translate(Math::Vector3(1000.0f, 0.0f, 0.0f));

            // 保留明确的场景迁移锚点，但不挂载 MeshRenderer，避免把当前不支持的效果伪装成正确输出。
            scene.CreateGameobject("Baseline.Pending.MultiMaterial");
            scene.CreateGameobject("Baseline.Pending.Transparent");

            const auto lightId = scene.CreateGameobject("Baseline.DirectionalLight");
            auto* light = scene.GetGameObject(lightId);
            light->transform->position = Math::Vector3(5.0f, 8.0f, 5.0f);
            light->transform->LookAt(Math::Vector3(0.3f, 0.3f, 0.3f));
            light->AddComponent<Framework::LightComponent>();
            light->GetComponent<Framework::LightComponent>()->color = Math::Vector3(1.0f, 0.95f, 0.9f);
        }

        /** @brief 读取可选的 `--project`，默认使用仓库根目录示例项目。 */
        std::filesystem::path ParseProjectPath(int argc, char** argv)
        {
            for (int index = 1; index + 1 < argc; ++index)
            {
                if (std::string_view(argv[index]) == "--project")
                    return argv[index + 1];
            }
            return "ChikaProject.json";
        }
    } // namespace

    class EditorApplication final : public Engine::Application
    {
      public:
        EditorApplication(Render::RenderPipelineMode pipelineMode, Render::RenderPathMode renderPath, Project::ProjectDescriptor descriptor) : m_pipelineMode(pipelineMode), m_renderPath(renderPath), m_descriptor(std::move(descriptor)) {}

      protected:
        Engine::EngineContextCreateInfo CreateEngineContextInfo() const override
        {
            Engine::EngineContextCreateInfo createInfo;
            createInfo.window.title = "Chika Engine";
            createInfo.window.width = 1280;
            createInfo.window.height = 720;
            createInfo.window.isFullscreen = false;
            createInfo.runtimeMode = Project::RuntimeMode::Editor;
            createInfo.contentRoot = (m_descriptor.projectRoot / m_descriptor.contentRoot).lexically_normal();
            createInfo.pythonEnvironmentRoot = (m_descriptor.projectRoot / ".venv").lexically_normal();
            createInfo.enableScripting = m_descriptor.runtime.enableScripting;
            createInfo.renderPipeline = m_pipelineMode;
            createInfo.renderPathMode = m_renderPath;
            return createInfo;
        }

        void OnInitialize() override
        {
            auto& context = GetEngineContext();
            auto* scene = context.GetScene();

            m_editor = std::make_unique<EditorManager>();
            m_editor->Initialize({
                .renderer = context.GetRenderer(),
                .window = context.GetWindow()->GetNativeHandle(),
                .sceneManager = context.GetSceneManager(),
                .scene = scene,
            });

            context.GetRenderer()->SetEnvironmentSettings(m_descriptor.runtime.environment);
            CreateRenderBaselineScene(*scene);
        }

        void OnUpdate(float deltaTime) override
        {
            m_editor->BeginFrame();
            m_editor->OnImGuiRender();
            m_editor->EndFrame();
            m_editor->Tick(deltaTime);
        }

        void OnShutdown() override
        {
            if (m_editor)
            {
                m_editor->Shutdown();
                m_editor.reset();
            }
        }

      private:
        std::unique_ptr<EditorManager> m_editor;
        Render::RenderPipelineMode m_pipelineMode = Render::RenderPipelineMode::Forward;
        Render::RenderPathMode m_renderPath = Render::RenderPathMode::JobCpu;
        Project::ProjectDescriptor m_descriptor;
    };
} // namespace ChikaEngine::Editor

int main(int argc, char** argv)
{
    ChikaEngine::Render::RenderPipelineMode pipelineMode = ChikaEngine::Render::RenderPipelineMode::Forward;
    ChikaEngine::Render::RenderPathMode renderPath = ChikaEngine::Render::RenderPathMode::JobCpu;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--deferred")
            pipelineMode = ChikaEngine::Render::RenderPipelineMode::Deferred;
        else if (argument == "--gpu-driven")
            renderPath = ChikaEngine::Render::RenderPathMode::GpuDriven;
    }

    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());
    ChikaEngine::Project::ProjectDescriptor descriptor;
    std::string error;
    if (!ChikaEngine::Project::ProjectDescriptor::Load(ChikaEngine::Editor::ParseProjectPath(argc, argv), descriptor, error))
    {
        LOG_ERROR("ChikaEditor", "Project load failed: {}", error);
        return 2;
    }

    ChikaEngine::Editor::EditorApplication application(pipelineMode, renderPath, std::move(descriptor));
    return application.Run();
}
