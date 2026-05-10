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
#include "glfw/glfw3.h"

#include "ChikaEngine/reflection/TypeRegister.h"
#include <iostream>
#include <memory>

int main()
{
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());
    try
    {
        using namespace ChikaEngine;
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(1280, 720, "Chika Engine", nullptr, nullptr);

        Core::UIDGenerator::Instance().Init(114);
        auto assetManager = std::make_unique<Asset::AssetManager>();
        auto render = std::make_unique<ChikaEngine::Render::Renderer>();
        ChikaEngine::Render::RendererCreateInfo info{ .windowHandle = window, .assetManager = assetManager.get(), .width = 1280, .height = 720 };
        render->Initialize(info);

        Framework::Scene scene;
        Framework::SceneCreateInfo sceneCreateInfo{ .renderInstance = render.get() };
        scene.Initialize(sceneCreateInfo);

        ChikaEngine::Reflection::InitAllReflection();

        std::unique_ptr<Editor::EditorManager> editor = std::make_unique<Editor::EditorManager>();
        Editor::EditorCreateInfo editorCreateInfo = {};
        editorCreateInfo.renderer = render.get();
        editorCreateInfo.window = window;
        editor->Initialize(editorCreateInfo);

        auto batmanID = scene.CreateGameobject("batman");
        scene.GetGameObject(batmanID)->AddComponent<Framework::MeshRenderer>("Assets/Meshes/Fox.gltf", "Assets/Materials/fox.json");
        scene.GetGameObject(batmanID)->transform->Scale(0.02f);
        scene.GetGameObject(batmanID)->transform->Translate(Math::Vector3(0.0f, 0.2f, 0.0f));
        scene.GetGameObject(batmanID)->transform->Rotate(Math::Vector3(0.0f, 0.5f, 0.0f));
        scene.GetGameObject(batmanID)->AddComponent<Framework::Animator>("Assets/Meshes/Fox.gltf");

        scene.GetGameObject(batmanID)->GetComponent<Framework::Animator>()->IsPlaying = true;
        auto planeId = scene.CreateGameobject("plane");
        scene.GetGameObject("plane")->AddComponent<Framework::MeshRenderer>("Assets/Meshes/Box.gltf", "Assets/Materials/floor.json");
        scene.GetGameObject("plane")->transform->Scale(10.0f, 0.1f, 10.f);

        double lastTime = glfwGetTime();
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            double now = glfwGetTime();
            float dt = float(now - lastTime);
            lastTime = now;

            editor->BeginFrame();
            editor->OnImGuiRender();
            editor->Tick(dt);
            render->BeginFrame();
            scene.Tick(dt);
            render->Tick(dt);
            render->EndFrame();
        }

        glfwDestroyWindow(window);
        glfwTerminate();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
}