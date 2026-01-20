#include "Editor.h"
#include "engine.h"
#include "render/renderer.h"
#include "window/window_desc.h"
#include "window/window_factory.h"
#include "window/window_system.h"

#include <memory>

int main()
{
    auto desc = ChikaEngine::Platform::WindowDesc();
    std::unique_ptr<ChikaEngine::Platform::IWindow> window = ChikaEngine::Platform::CreateWindow(desc, ChikaEngine::Platform::WindowBackend::GLFW);

    ChikaEngine::Engine::GameEngine engine;
    engine.Initialize(window.get());
    ChikaEngine::Editor::Editor editor(window.get());

    while (!window->ShouldClose())
    {
        engine.Tick();
        ChikaEngine::Render::Renderer::RenderObjectsToTarget(editor.ViewTargetHandle(), {engine.cube}, *editor.ViewCameraHandle());
        editor.Tick();
    }
    return 0;
}