#include "GLRHIDevice.h"

#include "GLRenderTarget.h"
#include "GLResource.h"
#include "glheader.h"


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
    IRHITexture2D* GLRHIDevice::CreateTexture2D(int width, int height, const void* data)
    {
        return new GLTexture2D(width, height, data);
    }
    IRHIPipeline* GLRHIDevice::CreatePipeline(const char* vsSource, const char* fsSource)
    {
        return new GLPipeline(vsSource, fsSource);
    }
    IRHIRenderTarget* GLRHIDevice::CreateRenderTarget(int width, int height)
    {
        return new GLRenderTarget(width, height);
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
        glBindVertexArray(0);
    }
} // namespace ChikaEngine::Render