#include "renderer.h"

#include "render_api.h"
#include "rhi/opengl/GLDevice.h"

#include <memory>
#include <stdexcept>

namespace ChikaEngine::Render
{
    void Renderer::Init(RenderAPI api, ::ChikaEngine::Platform::WindowSystem* window)
    {
        if (api == RenderAPI::None)
        {
            throw std::runtime_error("RenderAPI is None");
        }

        switch (api)
        {
        case RenderAPI::OpenGL:
            s_device = std::make_unique<GLDevice>(*window);
            break;
        default:
            throw std::runtime_error("Unknow RenderAPI");
        }
        s_device->Init();
    }

    void Renderer::Shutdown()
    {
        if (s_device)
        {
            s_device->Shutdown();
            s_device.reset();
        }
    }

    void Renderer::BeginFrame()
    {
        if (s_device)
            s_device->BeginFrame();
    }

    void Renderer::EndFrame()
    {
        if (s_device)
            s_device->EndFrame();
    }

    void Renderer::Submit(const RenderObject& obj)
    {
        if (s_device)
            s_device->DrawObject(obj);
    }
} // namespace ChikaEngine::Render