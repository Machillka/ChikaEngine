#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/debug/log_system.h"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/window/window_factory.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/renderer.h"
#include "ChikaEngine/scene/scene.h"
#include "ChikaEngine/window/window_desc.h"
#include "ChikaEngine/window/window_system.h"
#include "Editor.h"
#include "engine.h"
#include <cstdlib>
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
    // LOG_INFO("Main", "Engine initialized. Cube mesh={} material={}", engine.cube.mesh, engine.cube.material);
    ChikaEngine::Editor::Editor editor(window.get());
    // ChikaEngine::Framework::Temp* temp = new ChikaEngine::Framework::Temp();
    for (auto& ref : ChikaEngine::Reflection::TypeRegister::Instance()._registry)
    {
        LOG_INFO("Reflection", "Registered class: {} ({}), with {} properties and {} functions", ref.second.Name, ref.second.FullClassName, ref.second.Properties.size(), ref.second.Functions.size());

        for (const auto& prop : ref.second.Properties)
        {
            LOG_INFO("Reflection", "  Property: {} ({})", prop.Name, static_cast<int>(prop.Type));
        }
    }

    LOG_INFO("Main", "Entering main loop");
    while (!window->ShouldClose())
    {
        LOG_INFO("MainLoop", "Tick start");
        window->PollEvents();
        engine.Tick();
        // auto* class_info = ChikaEngine::Reflection::TypeRegister::Instance().GetClass("ChikaEngine::Framework::Temp");
        // class_info->FindFunction("Foo")->Invoke(temp, 42);
        ChikaEngine::Render::Renderer::RenderObjectsToTarget(editor.ViewTargetHandle(), ChikaEngine::Framework::Scene::Instance().GetAllVisiableRenderObjects(), editor.ViewCameraData());
        editor.Tick();
        window->SwapBuffers();
        LOG_INFO("MainLoop", "Tick end");
    }
    return 0;
}