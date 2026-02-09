#include "engine.h"

#include "InputDesc.h"
#include "InputSystem.h"
#include "TimeDesc.h"
#include "TimeSystem.h"
#include "debug/log_macros.h"
#include "framework/component/Renderable.h"
#include "framework/component/Transform.h"
#include "framework/scene/scene.h"
#include "include/Collider.h"
#include "math/vector3.h"
#include "physics/include/PhysicsDescs.h"
#include "physics/include/PhysicsSystem.h"
#include "physics/include/Rigidbody.h"
#include "render/renderer.h"
#include "window/window_system.h"
#include "reflection/TypeRegister.h"
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
        Physics::PhysicsSystem::Instance().Initialize(physicsDesc);

        // Resource system init and load assets from Assets/
        ChikaEngine::Resource::ResourceConfig cfg;
        cfg.assetRoot = "Assets";
        _resourceSystem.Init(cfg);

        // Load mesh and material from asset paths
        auto meshHandle = _resourceSystem.LoadMesh("Meshes/batman.obj");
        auto matHandle = _resourceSystem.LoadMaterial("Materials/suzanne.mat");

        LOG_INFO("Engine", "Loaded resources meshHandle={} matHandle={}", meshHandle, matHandle);
        LOG_INFO("Engine", "HasMesh={} HasMaterial={}", _resourceSystem.HasMesh("Meshes/batman.obj"), _resourceSystem.HasMaterial("Materials/suzanne.mat"));

        using namespace Framework;

        goTest = Framework::Scene::Instance().CreateGO("Batman");
        goTest->AddComponent<Collider>();
        goTest->AddComponent<Rigidbody>();
        goTest->AddComponent<Renderable>();
        goTest->GetComponent<Renderable>()->SetMaterial(matHandle);
        goTest->GetComponent<Renderable>()->SetMesh(meshHandle);

        meshHandle = _resourceSystem.LoadMesh("Meshes/cube.obj");

        auto plane = Framework::Scene::Instance().CreateGO("Cube");
        // plane->AddComponent<Collider>();
        plane->transform->Scale(2, 1, 2);
        plane->transform->Translate(Math::Vector3{0, -1, 0});
        plane->AddComponent<Renderable>();
        plane->GetComponent<Renderable>()->SetMaterial(matHandle);
        plane->GetComponent<Renderable>()->SetMesh(meshHandle);

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
            Physics::PhysicsSystem::Instance().Tick(fixedDt);
            accumulator -= fixedDt;
            ++steps;
        }

        auto pos = goTest->GetComponent<Framework::Transform>()->position;
        LOG_INFO("GO", "Position: x = {}, y = {}, z = {}", pos.x, pos.y, pos.z);

        Framework::Scene::Instance().Update(deltaTime);
        // Render::Renderer::RenderObjects({cube}, _camera);
    }
} // namespace ChikaEngine::Engine