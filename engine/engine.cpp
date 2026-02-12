#include "engine.h"
#include "ChikaEngine/Collider.h"
#include "ChikaEngine/InputDesc.h"
#include "ChikaEngine/InputSystem.h"
#include "ChikaEngine/PhysicsDescs.h"
#include "ChikaEngine/PhysicsSystem.h"
#include "ChikaEngine/ResourceSystem.h"
#include "ChikaEngine/Rigidbody.h"
#include "ChikaEngine/TimeSystem.h"
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/renderer.h"
#include "ChikaEngine/scene/scene.h"
#include "ChikaEngine/window/window_system.h"
#include <cstdlib>

namespace ChikaEngine::Engine
{
    GameEngine::GameEngine() = default;

    GameEngine::~GameEngine() = default;

    void GameEngine::Initialize(Platform::IWindow* window)
    {
        _window = window;
        /*====== 初始化各个系统 ======*/
        // Reflection
        ChikaEngine::Reflection::InitAllReflection();

        // Renderer
        ChikaEngine::Render::Renderer::Init(ChikaEngine::Render::RenderAPI::OpenGL);

        // Input
        ChikaEngine::Input::InputDesc inputDesc{.backendType = ChikaEngine::Input::InputBackendTypes::GLFW};
        ChikaEngine::Input::InputSystem::Init(inputDesc, window->GetNativeHandle());

        // Time
        ChikaEngine::Time::TimeSystem::Init(ChikaEngine::Time::TimeDesc{.backend = ChikaEngine::Time::TimeBackendTypes::GLFW});

        // Physics
        Physics::PhysicsSystemDesc physicsDesc{.initDesc = Physics::PhysicsInitDesc{}, .backendType = Physics::PhysicsBackendTypes::Jolt};
        Physics::PhysicsScene::Instance().Initialize(physicsDesc);

        // Resource system init and load assets from Assets/
        ChikaEngine::Resource::ResourceConfig cfg;
        cfg.assetRoot = "Assets";
        Resource::ResourceSystem::Instance().Init(cfg);

        // Load mesh and material from asset paths
        auto meshHandle = Resource::ResourceSystem::Instance().LoadMesh("Meshes/batman.obj");
        auto matHandle = Resource::ResourceSystem::Instance().LoadMaterial("Materials/suzanne.mat");
        LOG_INFO("Engine", "Loaded resources meshHandle={} matHandle={}", meshHandle, matHandle);
        LOG_INFO("Engine", "HasMesh={} HasMaterial={}", Resource::ResourceSystem::Instance().HasMesh("Meshes/batman.obj"), Resource::ResourceSystem::Instance().HasMaterial("Materials/suzanne.mat"));

        using namespace Framework;

        goTest = Framework::Scene::Instance().CreateGO("Batman");
        goTest->AddComponent<Collider>();
        goTest->AddComponent<Rigidbody>();
        goTest->AddComponent<Renderable>();
        goTest->GetComponent<Renderable>()->SetMaterial(matHandle);
        goTest->GetComponent<Renderable>()->SetMesh(meshHandle);

        meshHandle = Resource::ResourceSystem::Instance().LoadMesh("Meshes/cube.obj");

        // auto plane = Framework::Scene::Instance().CreateGO("Cube");
        // // plane->AddComponent<Collider>();
        // plane->transform->Scale(2, 1, 2);
        // plane->transform->Translate(Math::Vector3{0, -1, 0});
        // plane->AddComponent<Renderable>();
        // plane->GetComponent<Renderable>()->SetMaterial(matHandle);
        // plane->GetComponent<Renderable>()->SetMesh(meshHandle);
        // plane->GetComponent<Collider>()->halfExtents = Math::Vector3{10, 1, 10};
    }

    std::vector<Render::RenderObject>* GameEngine::RenderObjects()
    {
        return nullptr;
    }

    void GameEngine::Tick()
    {
        Time::TimeSystem::Update();
        float deltaTime = Time::TimeSystem::GetDeltaTime();

        Input::InputSystem::Update();

        // 计算是否需要 Update 物理
        static double accumulator = 0.0;
        accumulator += deltaTime;
        const float fixedDt = Time::TimeSystem::GetFixedDeltaTime();

        const int maxSteps = 5; // 限制物理更新,防止堵塞
        int steps = 0;
        while (accumulator >= fixedDt && steps < maxSteps)
        {
            // 提交引擎操作
            Framework::Scene::Instance().FixedUpdate(fixedDt);
            Physics::PhysicsScene::Instance().Tick(fixedDt);
            accumulator -= fixedDt;
            ++steps;
        }

        auto pos = goTest->GetComponent<Framework::Transform>()->position;
        LOG_INFO("GO", "Position: x = {}, y = {}, z = {}", pos.x, pos.y, pos.z);

        Framework::Scene::Instance().Update(deltaTime);
        // Render::Renderer::RenderObjects({cube}, _camera);
    }
} // namespace ChikaEngine::Engine