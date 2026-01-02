#include "debug/assert.h"
#include "debug/console_sink.h"
#include "debug/log_sink.h"
#include "debug/log_system.h"
#include "math/mat4.h"
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

#include <memory>
#include <numbers>
unsigned char whitePixel[4] = {255, 255, 255, 255};
const char* basicVS =
    R"( #version 330 core layout(location = 0) in vec3 aPos; layout(location = 1) in vec3 aNormal; layout(location = 2) in vec2 aUV; uniform mat4 uMVP; out vec2 vUV; void main() { vUV = aUV; gl_Position = uMVP * vec4(aPos, 1.0); } )";
const char* basicFS =
    R"( #version 330 core in vec2 vUV; out vec4 FragColor; uniform sampler2D uTex; void main() { FragColor = texture(uTex, vUV); } )";
int main()
{
    std::unique_ptr<ChikaEngine::Debug::ILogSink> consoleSink = std::make_unique<ChikaEngine::Debug::ConsoleLogSink>();
    ChikaEngine::Debug::LogSystem::Instance().AddSink(std::move(consoleSink));

    ChikaEngine::Platform::WindowDesc winDesc{
        .title = "Chika Engine Editor", .width = 1280, .height = 720, .vSync = false};
    //  创建窗口
    auto window = ChikaEngine::Platform::CreateWindow(winDesc, ChikaEngine::Platform::WindowBackend::GLFW);
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

    ChikaEngine::Render::Camera camera;

    while (!window->ShouldClose())
    {
        window->PollEvents();
        ChikaEngine::Render::Renderer::BeginFrame();
        ChikaEngine::Render::Renderer::RenderObjects({cube}, camera);
        ChikaEngine::Render::Renderer::EndFrame();
        window->SwapBuffers();
    }
    return 0;
}