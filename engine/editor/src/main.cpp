#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/debug/log_system.h"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/io/FileStream.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/renderer.h"
#include "ChikaEngine/serialization/JsonSaveArchive.h"
#include "ChikaEngine/window/window_factory.h"
#include "ChikaEngine/window/window_desc.h"
#include "ChikaEngine/window/window_system.h"
#include "Editor.h"
#include "ChikaEngine/scene/scene.h"
#include "engine.h"
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <vector>

int main()
{
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::make_unique<ChikaEngine::Debug::ConsoleLogSink>());

    LOG_INFO("Main", "Starting ChikaEditor");
    auto desc = ChikaEngine::Platform::WindowDesc();
    std::unique_ptr<ChikaEngine::Platform::IWindow> window = ChikaEngine::Platform::CreateWindow(desc, ChikaEngine::Platform::WindowBackend::GLFW);
    LOG_INFO("Main", "Window created");

    ChikaEngine::Engine::GameEngine engine;
    engine.Initialize(window.get());

    ChikaEngine::Editor::Editor editor;
    editor.Init(window.get());

    LOG_INFO("Main", "Entering main loop");

    auto a = engine.goTest;
    {
        ChikaEngine::IO::FileStream debugFile("vec.json", ChikaEngine::IO::Mode::Write);

        ChikaEngine::Serialization::JsonSaveArchive jsonAr(debugFile);
        jsonAr("scene", *a);
    }
    while (!window->ShouldClose())
    {
        // LOG_INFO("MainLoop", "Tick start");
        window->PollEvents();
        engine.Tick();
        // LOG_DEBUG("Camera", "y:{}", editor.ViewCameraData().position.y);

        // ChikaEngine::Render::Renderer::DrawSkybox(cubemapHandle, editor.ViewCameraData());
        ChikaEngine::Render::Renderer::RenderObjectsToTargetWithSkyBox(editor.ViewTargetHandle(), engine.mapcubeHanele, engine.GetActiveScene()->GetAllVisiableRenderObjects(), editor.ViewCameraData());
        editor.SetActiveScene(engine.GetActiveScene());
        editor.Tick();

        window->SwapBuffers();
        // LOG_INFO("MainLoop", "Tick end");
    }
    return 0;
}