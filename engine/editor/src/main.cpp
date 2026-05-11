#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/component/Animator.hpp"
#include "ChikaEngine/debug/log_system.h"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/scene/scene.hpp"
#include "EditorManager.hpp"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/Window/WindowFactory.hpp"
#include "ChikaEngine/Window/WinDesc.hpp"

#include <iostream>
#include <memory>
#include <chrono>

int main()
{
    // 初始化日志系统
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());

    try
    {
        using namespace ChikaEngine;

        Platform::WindowDesc windowDesc;
        windowDesc.title = "Chika Engine";
        windowDesc.width = 1280;
        windowDesc.height = 720;
        windowDesc.isFullscreen = false;
        auto window = Platform::WindowFactory::Create(windowDesc);
        Reflection::InitAllReflection();
        Core::UIDGenerator::Instance().Init(114);
        auto assetManager = std::make_unique<Asset::AssetManager>();

        auto render = std::make_unique<Render::Renderer>();
        Render::RendererCreateInfo rendererInfo;

        rendererInfo.windowHandle = window->GetNativeHandle();
        rendererInfo.width = window->GetWidth();
        rendererInfo.height = window->GetHeight();
        rendererInfo.assetManager = assetManager.get();

        render->Initialize(rendererInfo);

        window->SetResizeCallback([&render](uint32_t w, uint32_t h) { render->OnWindowResize(w, h); });

        Framework::Scene scene;
        Framework::SceneCreateInfo sceneCreateInfo{
            .renderInstance = render.get(),
        };
        scene.Initialize(sceneCreateInfo);

        // 初始化编辑器
        std::unique_ptr<Editor::EditorManager> editor = std::make_unique<Editor::EditorManager>();
        Editor::EditorCreateInfo editorCreateInfo = {};
        editorCreateInfo.renderer = render.get();
        editorCreateInfo.window = window->GetNativeHandle();
        editorCreateInfo.scene = &scene;
        editor->Initialize(editorCreateInfo);

        auto batmanID = scene.CreateGameobject("batman");
        scene.GetGameObject(batmanID)->AddComponent<Framework::MeshRenderer>("Assets/Meshes/Fox.gltf", "Assets/Materials/fox.json");
        scene.GetGameObject(batmanID)->transform->Scale(0.02f);
        scene.GetGameObject(batmanID)->transform->Translate(Math::Vector3(0.0f, 0.2f, 0.0f));
        scene.GetGameObject(batmanID)->transform->Rotate(Math::Vector3(0.0f, 0.5f, 0.0f));
        scene.GetGameObject(batmanID)->AddComponent<Framework::Animator>("Assets/Meshes/Fox.gltf");
        scene.GetGameObject(batmanID)->AddComponent<Framework::Rigidbody>();

        scene.GetGameObject(batmanID)->GetComponent<Framework::Animator>()->IsPlaying = true;
        auto planeId = scene.CreateGameobject("plane");
        scene.GetGameObject("plane")->AddComponent<Framework::MeshRenderer>("Assets/Meshes/Box.gltf", "Assets/Materials/floor.json");
        scene.GetGameObject("plane")->transform->Scale(10.0f, 0.1f, 10.f);

        auto lastTime = std::chrono::high_resolution_clock::now();

        while (!window->ShouldClose())
        {
            window->Tick();

            // 计算 DeltaTime
            auto currentTime = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // 编辑器帧
            editor->BeginFrame();
            editor->OnImGuiRender();
            editor->EndFrame();
            editor->Tick(dt);

            // 渲染与逻辑帧
            render->BeginFrame();
            scene.Tick(dt);

            render->Tick(dt);
            render->EndFrame();
        }
        render->GetRHIHandle()->WaitIdle();

        render->Shutdown();
        render.reset();
        editor.reset();
        assetManager.reset();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}