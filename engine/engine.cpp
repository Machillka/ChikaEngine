#include "engine.h"
#include "ChikaEngine/InputDesc.h"
#include "ChikaEngine/InputSystem.h"
#include "ChikaEngine/ResourceSystem.h"
#include "ChikaEngine/TimeSystem.h"
#include "ChikaEngine/base/UIDGenerator.h"
#include "ChikaEngine/component/Collider.h"
#include "ChikaEngine/component/Renderable.h"
#include "ChikaEngine/component/Rigidbody.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/renderer.h"
#include "ChikaEngine/scene/scene.h"
#include "ChikaEngine/window/window_system.h"
#include <cstdlib>
#include <memory>

namespace ChikaEngine::Engine
{
    GameEngine::GameEngine() : _scene(nullptr), _window(nullptr), goTest(nullptr) {};

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

        _scene = std::make_unique<Framework::Scene>(Framework::SceneMode::edit);

        ChikaEngine::Resource::ResourceConfig cfg;
        cfg.assetRoot = "Assets";
        auto ctx = Resource::ResourceSystem::Instance().LoadSettings();

        Core::UIDGenerator::Instance().Init(ctx.machineID);
        Resource::ResourceSystem::Instance().Init(ctx);

        // 加载天空盒
        using namespace ChikaEngine::Resource;

        TextureCubeSourceDesc skyDesc;

        skyDesc.facePaths = {
            "Textures/Skybox/nx.png", // Right
            "Textures/Skybox/nz.png", // Left
            "Textures/Skybox/ny.png", // Top
            "Textures/Skybox/px.png", // Bottom
            "Textures/Skybox/py.png", // Front
            "Textures/Skybox/pz.png"  // Back
        };
        skyDesc.sRGB = true;

        mapcubeHanele = ResourceSystem::Instance().LoadTextureCube(skyDesc);

        // auto meshHandle = Resource::ResourceSystem::Instance().LoadMesh("Meshes/batman.obj");
        // auto matHandle = Resource::ResourceSystem::Instance().LoadMaterial("Materials/suzanne.mat");

        auto meshHandle = Resource::ResourceSystem::Instance().LoadMesh("Meshes/batman.obj");
        auto matHandle = Resource::ResourceSystem::Instance().LoadMaterial("Materials/glass.mat");

        LOG_INFO("Engine", "HasMesh={} HasMaterial={}", Resource::ResourceSystem::Instance().HasMesh("Meshes/batman.obj"), Resource::ResourceSystem::Instance().HasMaterial("Materials/suzanne.mat"));

        using namespace Framework;
        auto id = _scene->CreateGameobject("Batman");
        goTest = _scene->GetGameobject(id);
        // goTest = Framework::Scene::Instance().CreateGO("Batman");
        goTest->AddComponent<Collider>();
        // goTest->AddComponent<Rigidbody>();
        goTest->AddComponent<Renderable>();
        goTest->GetComponent<Renderable>()->SetMaterial(matHandle);
        goTest->GetComponent<Renderable>()->SetMesh(meshHandle);

        meshHandle = Resource::ResourceSystem::Instance().LoadMesh("Meshes/cube.obj");

        // auto planeID = _scene->CreateGameobject("plane");
        // auto plane = _scene->GetGameobject(planeID);
        // plane->AddComponent<Collider>();
        // plane->transform->Scale(2, 1, 2);
        // plane->transform->Translate(Math::Vector3{0, -5, 0});
        // plane->AddComponent<Renderable>();
        // plane->GetComponent<Renderable>()->SetMaterial(matHandle);
        // plane->GetComponent<Renderable>()->SetMesh(meshHandle);
    }

    std::vector<Render::RenderObject>* GameEngine::RenderObjects()
    {
        return nullptr;
    }

    void GameEngine::Tick()
    {
        if (_scene == nullptr)
            return;

        Time::TimeSystem::Update();
        float deltaTime = Time::TimeSystem::GetDeltaTime();

        Input::InputSystem::Update();

        _scene->Update(deltaTime);

        // 计算是否需要 Update 物理
        static double accumulator = 0.0;
        accumulator += deltaTime;
        const float fixedDt = Time::TimeSystem::GetFixedDeltaTime();

        const int maxSteps = 5; // 限制物理更新,防止堵塞
        int steps = 0;
        while (accumulator >= fixedDt && steps < maxSteps)
        {
            // 提交引擎操作
            _scene->FixedUpdate(fixedDt);
            accumulator -= fixedDt;
            ++steps;
        }
    }

    /*!
     * @brief  实现把当前的场景进行序列化然后调整成 play mode
     *
     * @author Machillka (machillka2007@gmail.com)
     * @date 2026-02-27
     */
    void GameEngine::StartScene()
    {
        // TODO: 实现序列化备份数据
        _scene = std::make_unique<Framework::Scene>(Framework::SceneMode::play);
    }

    void GameEngine::EndScene()
    {
        _scene.reset();
        _scene = nullptr;
    }

    Framework::Scene* GameEngine::GetActiveScene() const
    {
        return _scene.get();
    }
} // namespace ChikaEngine::Engine