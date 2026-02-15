#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/debug/log_system.h"
#include "ChikaEngine/debug/console_sink.h"
#include "ChikaEngine/io/FileStream.h"
#include "ChikaEngine/io/IStream.h"
#include "ChikaEngine/io/MemoryStream.h"
#include "ChikaEngine/math/vector3.h"
#include "ChikaEngine/serialization/BinaryLoadArchive.h"
#include "ChikaEngine/serialization/JsonLoadArchive.h"
#include "ChikaEngine/serialization/JsonSaveArchive.h"
#include "ChikaEngine/window/window_factory.h"
#include "ChikaEngine/reflection/TypeRegister.h"
#include "ChikaEngine/window/window_desc.h"
#include "ChikaEngine/window/window_system.h"
#include "Editor.h"

#include "ChikaEngine/serialization/BinarySaveArchive.h"
#include "engine.h"
#include <cstddef>
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
    ChikaEngine::Editor::Editor editor;
    editor.Init(window.get());
    // ChikaEngine::Framework::Temp* temp = new ChikaEngine::Framework::Temp();
    for (auto& ref : ChikaEngine::Reflection::TypeRegister::Instance()._registryFullName)
    {
        LOG_INFO("Reflection", "Registered class: {} ({}), with {} properties and {} functions", ref.second.Name, ref.second.FullClassName, ref.second.Properties.size(), ref.second.Functions.size());

        for (const auto& prop : ref.second.Properties)
        {
            LOG_INFO("Reflection", "  Property: {} ({})", prop.Name, static_cast<int>(prop.Type));
        }
    }

    LOG_INFO("Main", "Entering main loop");
    using namespace ChikaEngine;
    IO::MemoryStream fs;
    Math::Vector3 test = {5, 1, 4};
    {

        LOG_DEBUG("io", "create");
        Serialization::BinarySaveArchive archive(fs);
        LOG_DEBUG("io", "archived crated");
        archive(test);
        LOG_DEBUG("main", "memorystrem, length = {}", fs.GetLength());
    }
    Math::Vector3 temp;
    {
        fs.FlipToRead();
        Serialization::BinaryLoadArchive archive(fs);
        archive("fuck",temp);
    }
    {
        IO::FileStream outStream("test.json", IO::Mode::Write);

        Serialization::JsonSaveArchive archive(outStream);
        archive("fuck", test);
        size_t len = outStream.GetLength();

        std::string str;
        str.resize(len);
        outStream.Read(str.data(), len);
    }

    LOG_DEBUG("Main Fuck", "x:{}, y:{}, z:{}", temp.x, temp.y, temp.z);
    // while (!window->ShouldClose())
    // {
    //     LOG_INFO("MainLoop", "Tick start");
    //     window->PollEvents();
    //     // engine.Tick();
    //     // ChikaEngine::Render::Renderer::RenderObjectsToTarget(editor.ViewTargetHandle(), engine._scene->GetAllVisiableRenderObjects(), editor.ViewCameraData());
    //     window->SwapBuffers();
    //     LOG_INFO("MainLoop", "Tick end");
    // }
    return 0;
}