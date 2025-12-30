#include "GLDevice.h"

#include "math/mat4.h"
#include "platform/window/window_system.h"
#include "render/rhi/opengl/GLPipeline.h"
#include "render/rhi/opengl/GLShader.h"
#include "render/rhi/opengl/GLVertexArray.h"
#include "render/rhi/opengl/glheader.h"

namespace ChikaEngine::Render
{
    GLDevice::GLDevice(::ChikaEngine::Platform::IWindowSystem& window) noexcept : _window(window) {}
    GLDevice::~GLDevice()
    {
        Shutdown();
    }

    void GLDevice::Init()
    {
        glEnable(GL_DEPTH_TEST);
        OnResize(_window.GetWidth(), _window.GetHeight());
    }

    void GLDevice::Shutdown() {}

    void GLDevice::OnResize(const std::uint32_t width, const std::uint32_t height)
    {
        glViewport(0, 0, static_cast<int>(width), static_cast<int>(height));
    }

    std::shared_ptr<IRHIVertexArray> GLDevice::CreateVertexArray(const MeshData& mesh)
    {
        return std::make_shared<GLVertexArray>(mesh);
    }

    std::shared_ptr<IRHIShader> GLDevice::CreateShader(const ShaderSource& source)
    {
        return std::make_shared<GLShader>(source);
    }
    std::shared_ptr<IRHIPipeline> GLDevice::CreatePipeline(std::shared_ptr<IRHIShader> shader)
    {
        return std::make_shared<GLPipeline>(std::move(shader));
    }

    void GLDevice::BeginFrame()
    {
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void GLDevice::DrawVertexArray(const IRHIVertexArray& vao, const std::uint32_t indexCount)
    {
        auto& glVAO = static_cast<const GLVertexArray&>(vao);
        glVAO.Bind();
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
        glVAO.UnBind();
    }

    void GLDevice::EndFrame() {}
    void GLDevice::DrawObject(const RenderObject& obj)
    {
        if (!obj.VAO || !obj.pipeline || !obj.material || !obj.camera)
            return;
        auto pipeline = static_cast<const GLPipeline*>(obj.pipeline);
        auto shader = std::static_pointer_cast<IRHIShader>(pipeline->GetShader());
        pipeline->Bind();
        const Math::Mat4 vp = obj.camera->ViewProjectionMat();
        const Math::Mat4 mvp = vp * obj.modelMat;
        shader->SetMat4("u_MVP", mvp);
        shader->SetVec3("u_BaseColor", obj.material->BaseColor());
        DrawVertexArray(*obj.VAO, static_cast<const GLVertexArray*>(obj.VAO)->IndexCount());
        pipeline->UnBind();
    }
} // namespace ChikaEngine::Render