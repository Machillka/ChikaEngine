#include "Editor.h"
#include "debug/console_sink.h"
#include "debug/log_macros.h"
#include "engine.h"
#include "render/renderer.h"
#include "window/window_desc.h"
#include "window/window_factory.h"
#include "window/window_system.h"

#include <memory>

int main()
{
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());

    LOG_INFO("Main", "Starting ChikaEditor");
    auto desc = ChikaEngine::Platform::WindowDesc();
    std::unique_ptr<ChikaEngine::Platform::IWindow> window = ChikaEngine::Platform::CreateWindow(desc, ChikaEngine::Platform::WindowBackend::GLFW);
    LOG_INFO("Main", "Window created");

    ChikaEngine::Engine::GameEngine engine;
    engine.Initialize(window.get());
    LOG_INFO("Main", "Engine initialized. Cube mesh={} material={}", engine.cube.mesh, engine.cube.material);
    ChikaEngine::Editor::Editor editor(window.get());

    LOG_INFO("Main", "Entering main loop");
    while (!window->ShouldClose())
    {
        // LOG_INFO("MainLoop", "Tick start");
        window->PollEvents();
        engine.Tick();
        ChikaEngine::Render::Renderer::RenderObjectsToTarget(editor.ViewTargetHandle(), {engine.cube}, *editor.ViewCameraHandle());
        editor.Tick();
        window->SwapBuffers();
        // LOG_INFO("MainLoop", "Tick end");
    }
    return 0;
}