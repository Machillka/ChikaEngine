#include "ChikaEngine/Application.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/Window/IWindow.hpp"
#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/debug/log_system.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/scene/scene.hpp"
#include "EditorManager.hpp"
#include <memory>

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

            // 保留明确的场景迁移锚点，但不挂载 MeshRenderer，避免把当前不支持的效果伪装成正确输出。
            scene.CreateGameobject("Baseline.Pending.MultiMaterial");
            scene.CreateGameobject("Baseline.Pending.Transparent");
        }
    } // namespace

    class EditorApplication final : public Engine::Application
    {
      protected:
        Engine::EngineContextCreateInfo CreateEngineContextInfo() const override
        {
            Engine::EngineContextCreateInfo createInfo;
            createInfo.window.title = "Chika Engine";
            createInfo.window.width = 1280;
            createInfo.window.height = 720;
            createInfo.window.isFullscreen = false;
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
    };
} // namespace ChikaEngine::Editor

int main()
{
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());
    ChikaEngine::Editor::EditorApplication application;
    return application.Run();
}
