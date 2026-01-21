#include "engine.h"

#include "InputDesc.h"
#include "InputSystem.h"
#include "TimeDesc.h"
#include "TimeSystem.h"
#include "debug/log_macros.h"
#include "render/renderer.h"
#include "window/window_system.h"

namespace ChikaEngine::Engine
{
    GameEngine::GameEngine() = default;

    GameEngine::~GameEngine() = default;

    void GameEngine::Initialize(Platform::IWindow* window)
    {
        _window = window;
        /*====== 初始化各个系统 ======*/

        // Renderer
        ChikaEngine::Render::Renderer::Init(ChikaEngine::Render::RenderAPI::OpenGL);

        // Input
        ChikaEngine::Input::InputDesc desc{.backendType = ChikaEngine::Input::InputBackendTypes::GLFW};
        ChikaEngine::Input::InputSystem::Init(desc, window->GetNativeHandle());

        // Time
        ChikaEngine::Time::TimeSystem::Init(ChikaEngine::Time::TimeDesc{.backend = ChikaEngine::Time::TimeBackendTypes::GLFW});

        // Resource system init and load assets from Assets/
        ChikaEngine::Resource::ResourceConfig cfg;
        cfg.assetRoot = "Assets";
        _resourceSystem.Init(cfg);

        // try
        // {
        // Load mesh and material from asset paths
        auto meshHandle = _resourceSystem.LoadMesh("Meshes/dragon.obj");
        auto matHandle = _resourceSystem.LoadMaterial("Materials/test.mat");

        LOG_INFO("Engine", "Loaded resources meshHandle={} matHandle={}", meshHandle, matHandle);
        LOG_INFO("Engine", "HasMesh={} HasMaterial={}", _resourceSystem.HasMesh("Meshes/test.obj"), _resourceSystem.HasMaterial("Materials/test.mat"));

        Render::RenderObject ro;
        ro.mesh = meshHandle;
        ro.material = matHandle;
        cube = ro;
        // }
        // catch (const std::exception& ex)
        // {
        //     std::string msg = std::string("Failed to load resources: ") + ex.what() + ". Using prefab cube.";
        //     LOG_ERROR("Engine", msg.c_str());
        //     cube = Render::Renderer::CreateRO(Render::RenderObjectPrefabs::Cube);
        // }
    }

    std::vector<Render::RenderObject>* GameEngine::RenderObjects()
    {
        return nullptr;
    }

    void GameEngine::Tick()
    {
        float deltaTime = Time::TimeSystem::GetDeltaTime();

        Time::TimeSystem::Update();
        Input::InputSystem::Update();
        // Render::Renderer::RenderObjects({cube}, _camera);
    }
} // namespace ChikaEngine::Engine