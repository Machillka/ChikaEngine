#include "renderer.h"

#include "Resource/ShaderPool.h"
#include "debug/assert.h"
#include "math/ChikaMath.h"
#include "math/mat4.h"
#include "render/Resource/MaterialPool.h"
#include "render/Resource/MeshLibrary.h"
#include "render/Resource/MeshPool.h"
#include "render/Resource/Shader.h"
#include "render/rhi/opengl/GLRHIDevice.h"
#include "render/rhi/opengl/GLRenderDevice.h"
#include "render_api.h"
#include "renderobject.h"

#include <memory>
#include <stdexcept>

namespace ChikaEngine::Render
{
    void Renderer::Init(RenderAPI api)
    {
        if (api == RenderAPI::None)
        {
            throw std::runtime_error("RenderAPI is None");
        }

        switch (api)
        {
        case RenderAPI::OpenGL:
            _rhiDevice = std::make_unique<GLRHIDevice>();
            CHIKA_ASSERT(_rhiDevice != nullptr, "null rhi device");
            _renderDevice = std::make_unique<GLRenderDevice>(_rhiDevice.get());
            break;
        default:
            throw std::runtime_error("Unknow RenderAPI");
        }
        _renderDevice->Init();
    }

    void Renderer::Shutdown()
    {
        if (_renderDevice)
        {
            _renderDevice->Shutdown();
            _renderDevice.reset();
            _rhiDevice.reset();
        }
    }

    void Renderer::BeginFrame()
    {
        if (_renderDevice)
            _renderDevice->BeginFrame();
    }

    void Renderer::EndFrame()
    {
        if (_renderDevice)
            _renderDevice->EndFrame();
    }

    void Renderer::RenderObjects(const std::vector<RenderObject>& ros, const Camera& camera)
    {
        if (!_renderDevice)
            return;
        Math::Mat4 view = camera.ViewMat();
        Math::Mat4 proj = camera.ProjectionMat();
        for (const auto& obj : ros)
        {
            _renderDevice->DrawObject(obj, view, proj);
        }
    }
    void Renderer::RenderObjectsToTarget(IRHIRenderTarget* target, const std::vector<RenderObject>& ros, const Camera& camera)
    {
        if (!target || !_renderDevice)
            return;
        target->Bind();

        _renderDevice->BeginFrame();
        Math::Mat4 view = camera.ViewMat();
        Math::Mat4 proj = camera.ProjectionMat();
        for (const auto& obj : ros)
        {
            _renderDevice->DrawObject(obj, view, proj);
        }
        _renderDevice->EndFrame();

        target->UnBind();
    }
    IRHIRenderTarget* Renderer::CreateRenderTarget(int width, int height)
    {
        return _rhiDevice->CreateRenderTarget(width, height);
    }

    RenderObject Renderer::CreateRO(RenderObjectPrefabs prefab)
    {
        switch (prefab)
        {
        case RenderObjectPrefabs::Cube:
            return CreateCubeRO();
        default:
            return CreateCubeRO();
        }
    }

    RenderObject Renderer::CreateCubeRO()
    {
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
        ChikaEngine::Render::Shader shader;
        shader.vertexSource = basicVS;
        shader.fragmentSource = basicFS;
        auto shaderHandle = ChikaEngine::Render::ShaderPool::Create(shader);

        auto cubeMesh = ChikaEngine::Render::MeshLibrary::Cube();
        auto meshHandle = ChikaEngine::Render::MeshPool::Create(cubeMesh);
        ChikaEngine::Render::Material mat(shaderHandle);
        unsigned char whitePixel[4] = {255, 255, 255, 255};
        auto materialHandle = ChikaEngine::Render::MaterialPool::Create(mat, whitePixel, 1, 1);

        ChikaEngine::Render::RenderObject cube;
        cube.mesh = meshHandle;
        cube.material = materialHandle;

        return cube;
    }
} // namespace ChikaEngine::Render