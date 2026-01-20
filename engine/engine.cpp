#include "engine.h"

#include "InputDesc.h"
#include "InputSystem.h"
#include "TimeDesc.h"
#include "TimeSystem.h"
#include "render/renderer.h"
#include "window/window_system.h"

namespace ChikaEngine::Engine
{
    GameEngine::GameEngine()
    {
        // default ctor
    }

    GameEngine::~GameEngine()
    {
        // default dtor
    }

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

        cube = Render::Renderer::CreateRO(Render::RenderObjectPrefabs::Cube);
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