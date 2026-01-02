#include "renderer.h"

#include "debug/assert.h"
#include "math/mat4.h"
#include "render/rhi/opengl/GLRHIDevice.h"
#include "render/rhi/opengl/GLRenderDevice.h"
#include "render_api.h"

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
} // namespace ChikaEngine::Render