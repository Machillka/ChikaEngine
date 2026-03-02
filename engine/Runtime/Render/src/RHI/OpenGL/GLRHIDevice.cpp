
#include "ChikaEngine/RHI/OpenGL/GLRHIDevice.h"
#include "ChikaEngine/RHI/OpenGL/GLBuffer.h"
#include "ChikaEngine/RHI/OpenGL/GLPipeline.h"
#include "ChikaEngine/RHI/OpenGL/GLRenderTarget.h"
#include "ChikaEngine/RHI/OpenGL/GLTexture2D.h"
#include "ChikaEngine/RHI/OpenGL/GLTextureCube.h"
#include "ChikaEngine/RHI/OpenGL/GLVertexArray.h"
#include "ChikaEngine/Resource/Mesh.h"

namespace ChikaEngine::Render
{
    GLRHIDevice::~GLRHIDevice() {}
    void GLRHIDevice::BeginFrame()
    {
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // 设置清屏颜色
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST); // 启用深度测试
    }
    void GLRHIDevice::EndFrame() {}
    IRHIVertexArray* GLRHIDevice::CreateVertexArray()
    {
        return new GLVertexArray();
    }
    IRHIBuffer* GLRHIDevice::CreateVertexBuffer(std::size_t size, const void* data)
    {
        return new GLBuffer(GLBufferType::Vertex, size, data);
    }
    IRHIBuffer* GLRHIDevice::CreateIndexBuffer(std::size_t size, const void* data)
    {
        return new GLBuffer(GLBufferType::Index, size, data);
    }
    IRHITexture2D* GLRHIDevice::CreateTexture2D(int width, int height, int channels, const void* data, bool sRGB)
    {
        return new GLTexture2D(width, height, channels, data, sRGB);
    }
    IRHIPipeline* GLRHIDevice::CreatePipeline(const char* vsSource, const char* fsSource)
    {
        return new GLPipeline(vsSource, fsSource);
    }
    IRHIRenderTarget* GLRHIDevice::CreateRenderTarget(int width, int height)
    {
        return new GLRenderTarget(width, height);
    }

    IRHITextureCube* GLRHIDevice::CreateTextureCube(int w, int h, int channels, const std::array<const void*, 6>& data, bool sRGB)
    {
        return new GLTextureCube(w, h, channels, data, sRGB);
    }

    void GLRHIDevice::DrawIndexed(const RHIMesh& mesh)
    {
        auto* vao = static_cast<const GLVertexArray*>(mesh.vao);
        glBindVertexArray(vao->Handle());

        GLenum indexType = (mesh.indexType == IndexType::Uint16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        glDrawElements(GL_TRIANGLES, mesh.indexCount, indexType, nullptr);
    }
    void GLRHIDevice::SetupMeshVertexLayout(IRHIVertexArray* vao, IRHIBuffer* vbo, IRHIBuffer* ibo)
    {
        auto* glVao = static_cast<GLVertexArray*>(vao);
        auto* glVbo = static_cast<GLBuffer*>(vbo);
        auto* glIbo = static_cast<GLBuffer*>(ibo);
        glBindVertexArray(glVao->Handle());
        glBindBuffer(GL_ARRAY_BUFFER, glVbo->Handle());
        constexpr GLsizei stride = sizeof(float) * 8;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glIbo->Handle());

        if (ibo != nullptr)
        {
            auto* glIbo = static_cast<GLBuffer*>(ibo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glIbo->Handle());
        }

        glBindVertexArray(0);
    }
    void GLRHIDevice::SetupGizmoVertexLayout(IRHIVertexArray* vao, IRHIBuffer* vbo)
    {
        auto* glVao = static_cast<GLVertexArray*>(vao);
        auto* glVbo = static_cast<GLBuffer*>(vbo);

        glBindVertexArray(glVao->Handle());
        glBindBuffer(GL_ARRAY_BUFFER, glVbo->Handle());

        // Gizmo 专属布局: Position(3) + Color(4) = 7 floats
        constexpr GLsizei stride = sizeof(float) * 7;

        // Location 0: aPos (vec3)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

        // Location 1: aColor (vec4)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));

        glBindVertexArray(0);
    }
    void GLRHIDevice::UpdateBufferData(IRHIBuffer* buffer, std::size_t size, const void* data, std::size_t offset)
    {
        if (!buffer || !data || size == 0)
            return;
        auto* glBuf = static_cast<GLBuffer*>(buffer);

        glBindBuffer(GL_ARRAY_BUFFER, glBuf->Handle());
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    void GLRHIDevice::DrawLines(IRHIVertexArray* vao, std::uint32_t vertexCount, std::uint32_t firstVertex)
    {
        if (!vao)
            return;
        auto* glVao = static_cast<GLVertexArray*>(vao);

        glBindVertexArray(glVao->Handle());
        glDrawArrays(GL_LINES, firstVertex, vertexCount);
        glBindVertexArray(0);
        glDrawArrays(GL_LINES, firstVertex, vertexCount);
    }
} // namespace ChikaEngine::Render