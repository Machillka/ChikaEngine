#include "Editor.h"
#include "GLFW/glfw3.h"
#include "InputDesc.h"
#include "debug/console_sink.h"
#include "debug/editor_sink.h"
#include "debug/log_macros.h"
#include "debug/log_sink.h"
#include "debug/log_system.h"
#include "editor/adapter/imgui/ImguiAdapter.h"
#include "editor/adapter/imgui/ImguiLogPanel.h"
#include "input/GlfwInputBackend.h"
#include "input/InputCodes.h"
#include "input/InputSystem.h"
#include "math/vector3.h"
#include "platform/window/window_desc.h"
#include "platform/window/window_factory.h"
#include "render/Resource/MaterialPool.h"
#include "render/Resource/MeshLibrary.h"
#include "render/Resource/MeshPool.h"
#include "render/Resource/Shader.h"
#include "render/Resource/ShaderPool.h"
#include "render/camera.h"
#include "render/render_api.h"
#include "render/renderer.h"
#include "render/renderobject.h"
#include "time/TimeDesc.h"
#include "time/TimeSystem.h"

#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <numbers>
#include <sstream>
#include <utility>

unsigned char whitePixel[4] = {255, 255, 255, 255};
const char* basicVS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main() { vUV = aUV; gl_Position = uMVP * vec4(aPos, 1.0); }
)";
const char* basicFS = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main() { FragColor = texture(uTex, vUV); }
)";
void UpdateCamera(ChikaEngine::Render::Camera& camera, float deltaTime);
int main()
{
    std::unique_ptr<ChikaEngine::Debug::ILogSink> consoleSink = std::make_unique<ChikaEngine::Debug::ConsoleLogSink>();
    std::unique_ptr<ChikaEngine::Debug::ILogSink> editorSink = std::make_unique<ChikaEngine::Debug::EditorLogSink>();
    auto editorSinkRaw = editorSink.get();
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::move(consoleSink));
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::move(editorSink));
    ChikaEngine::Platform::WindowDesc winDesc{.title = "Chika Engine Editor", .width = 1280, .height = 720, .vSync = false};
    //  创建窗口
    auto window = ChikaEngine::Platform::CreateWindow(winDesc, ChikaEngine::Platform::WindowBackend::GLFW);
    auto windowHandle = window->GetNativeHandle();
    ChikaEngine::Editor::Editor editor;
    auto ui = std::make_unique<ChikaEngine::Editor::ImGuiAdapter>(windowHandle);
    auto logPanel = std::make_unique<ChikaEngine::Editor::ImguiLogPanel>();
    logPanel->SetEditorSink(static_cast<ChikaEngine::Debug::EditorLogSink*>(editorSinkRaw));
    editor.Init(std::move(ui), windowHandle);
    editor.RegisterPanel(std::move(logPanel));

    // 初始化渲染器
    ChikaEngine::Render::Renderer::Init(ChikaEngine::Render::RenderAPI::OpenGL);

    // 生成美术资产
    ChikaEngine::Render::Shader shader;
    shader.vertexSource = basicVS;
    shader.fragmentSource = basicFS;
    auto shaderHandle = ChikaEngine::Render::ShaderPool::Create(shader);

    auto cubeMesh = ChikaEngine::Render::MeshLibrary::Cube();
    auto meshHandle = ChikaEngine::Render::MeshPool::Create(cubeMesh);
    ChikaEngine::Render::Material mat;
    mat.shaderHandle = shaderHandle;
    mat.albedoTexture = 0;
    mat.albedoColor = {1, 1, 1, 1};
    auto materialHandle = ChikaEngine::Render::MaterialPool::Create(mat, whitePixel, 1, 1);

    ChikaEngine::Render::RenderObject cube;
    cube.mesh = meshHandle;
    cube.material = materialHandle;
    cube.modelMat = ChikaEngine::Math::Mat4::Identity();
    cube.modelMat.RotateX(std::numbers::pi / 3);
    cube.modelMat.RotateY(std::numbers::pi / 3);
    // ChikaEngine::Input::InputSystem::Init(std::make_unique<ChikaEngine::Input::GlfwInputBackend>(static_cast<GLFWwindow*>(windowHandle)));
    ChikaEngine::Input::InputDesc desc{.backendType = ChikaEngine::Input::InputBackendTypes::GLFW};
    ChikaEngine::Input::InputSystem::Init(desc, windowHandle);
    // ChikaEngine::Input::InputDesc inputDesc = {ChikaEngine::Input::InputBackendTypes::GLFW};
    // ChikaEngine::Input::InputSystem::Init(inputDesc, windowHandle);

    ChikaEngine::Render::Camera camera;
    ChikaEngine::Time::TimeSystem::Init(ChikaEngine::Time::TimeDesc{.backend = ChikaEngine::Time::TimeBackendTypes::GLFW});
    using Time = ChikaEngine::Time::TimeSystem;
    while (!window->ShouldClose())
    {
        // 时间更新
        // TODO: time system
        double currentTime = glfwGetTime();
        float deltaTime = Time::GetDeltaTime();
        window->PollEvents();
        ChikaEngine::Input::InputSystem::Update();

        std::stringstream ss;
        ss << "DeltaTime: " << Time::GetDeltaTime() << "FPS:" << Time::GetFPS() << std::endl;
        LOG_INFO("time", ss.str());
        editor.Tick();
        Time::Update();
        UpdateCamera(camera, deltaTime);
        ChikaEngine::Render::Renderer::BeginFrame();
        ChikaEngine::Render::Renderer::RenderObjects({cube}, camera);
        ChikaEngine::Render::Renderer::EndFrame();
        window->SwapBuffers();
    }
    return 0;
}

void UpdateCamera(ChikaEngine::Render::Camera& camera, float deltaTime)
{
    const float speed = 5.0f;
    ChikaEngine::Math::Vector3 move{};

    using namespace ChikaEngine::Input;
    if (InputSystem::GetKey(KeyCode::W))
        move += camera.Front();
    if (InputSystem::GetKey(KeyCode::S))
        move -= camera.Front();
    if (InputSystem::GetKey(KeyCode::A))
        move -= camera.Front().Cross(ChikaEngine::Math::Vector3::up).Normalized();
    if (InputSystem::GetKey(KeyCode::D))
        move += camera.Front().Cross(ChikaEngine::Math::Vector3::up).Normalized();
    // vertical movement
    if (InputSystem::GetKey(KeyCode::Space))
        move += ChikaEngine::Math::Vector3::up;
    if (InputSystem::GetKey(KeyCode::LeftShift))
        move -= ChikaEngine::Math::Vector3::up;

    if (move.Length() > 0.0f)
        camera.transform->Translate(move.Normalized() * speed * deltaTime);

    // 鼠标视角（右键按住时）
    if (InputSystem::GetMouseButton(ChikaEngine::Input::MouseButton::Right))
    {
        auto [dx, dy] = InputSystem::GetMouseDelta();
        camera.ProcessMouseMovement(static_cast<float>(dx), static_cast<float>(dy));
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "Mouse Δ: (" << dx << ", " << dy << "), Pos: (" << InputSystem::GetMousePosition().first << ", " << InputSystem::GetMousePosition().second << ")";
        LOG_INFO("main", ss.str());
    }
}
