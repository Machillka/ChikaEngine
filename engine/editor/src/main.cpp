#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/MeshRenderer.h"
#include "ChikaEngine/component/Rigidbody.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/debug/log_system.h"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/scene/scene.hpp"
#include "glfw/glfw3.h"
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
        GLFWwindow* window = glfwCreateWindow(1280, 720, "Chika Engine - Pure Renderer Test", nullptr, nullptr);

        Core::UIDGenerator::Instance().Init(114);

        auto assetManager = std::make_unique<Asset::AssetManager>();

        auto render = std::make_unique<ChikaEngine::Render::Renderer>();
        ChikaEngine::Render::RendererCreateInfo info{ .windowHandle = window, .assetManager = assetManager.get(), .width = 1280, .height = 720 };
        render->Initialize(info);

        Framework::Scene scene;
        Framework::SceneCreateInfo sceneCreateInfo{ .renderInstance = render.get() };
        scene.Initialize(sceneCreateInfo);

        auto batmanID = scene.CreateGameobject("batman");
        scene.GetGameObject(batmanID)->AddComponent<Framework::MeshRenderer>("Assets/Meshes/batman.obj", "Assets/Materials/test.json");
        scene.GetGameObject(batmanID)->transform->Translate(0.0f, 0.0f, 1.0f);
        // scene.GetGameObject(batmanID)->AddComponent<Framework::Rigidbody>();

        auto planeId = scene.CreateGameobject("plane");
        scene.GetGameObject("plane")->AddComponent<Framework::MeshRenderer>("Assets/Meshes/cube.obj", "Assets/Materials/floor.json");
        scene.GetGameObject("plane")->transform->Scale(10.0f, 0.1f, 10.f);
        scene.GetGameObject("plane")->AddComponent<Framework::Rigidbody>();
        LOG_INFO("Main", "Plane ID = {}", planeId);
        double lastTime = glfwGetTime();
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            double now = glfwGetTime();
            float dt = float(now - lastTime);
            lastTime = now;
            render->BeginFrame();
            scene.GetGameObject(batmanID)->transform->Rotate(Math::Vector3(0.0f, 1.0f, 0.0f) * dt);
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