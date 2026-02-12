
#include "ChikaEngine/renderer.h"
#include "ChikaEngine/RHI/OpenGL/GLRHIDevice.h"
#include "ChikaEngine/debug/assert.h"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/render_device.h"
#include "ChikaEngine/RHI/OpenGL/GLRenderDevice.h"

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

    void Renderer::RenderObjects(const std::vector<RenderObject>& ros, const CameraData& camera)
    {
        if (!_renderDevice)
            return;
        Math::Mat4 view = camera.viewMatrix;
        Math::Mat4 proj = camera.projectionMatrix;
        for (const auto& obj : ros)
        {
            _renderDevice->DrawObject(obj, camera);
        }
    }
    void Renderer::RenderObjectsToTarget(IRHIRenderTarget* target, const std::vector<RenderObject>& ros, const CameraData& camera)
    {
        if (!target || !_renderDevice)
            return;

        LOG_INFO("Renderer", "Rendering {} objects", ros.size());

        target->Bind();

        _renderDevice->BeginFrame();
        Math::Mat4 view = camera.viewMatrix;
        Math::Mat4 proj = camera.projectionMatrix;
        for (const auto& obj : ros)
        {
            _renderDevice->DrawObject(obj, camera);
        }
        _renderDevice->EndFrame();

        target->UnBind();
    }
    IRHIRenderTarget* Renderer::CreateRenderTarget(int width, int height)
    {
        return _rhiDevice->CreateRenderTarget(width, height);
    }

} // namespace ChikaEngine::Render