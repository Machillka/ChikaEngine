#include "GLRHIDevice.h"

#include "GLHeader.h"
#include "GLResource.h"
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
    void GLRHIDevice::DrawIndexed(const DrawIndexedCommand& cmd)
    {
        auto* vao = static_cast<const GLVertexArray*>(cmd.vao);
        auto* pipe = static_cast<const GLPipeline*>(cmd.pipe);
        auto* tex = static_cast<const GLTexture2D*>(cmd.tex);
        glUseProgram(pipe->Program());

        // 传递 MVP 矩阵到 shader
        GLint mvpLoc = glGetUniformLocation(pipe->Program(), "uMVP");
        if (mvpLoc != -1 && cmd.mvpMatrix != nullptr)
        {
            // GL_TRUE: 因为 Mat4 使用行优先存储，OpenGL 期望列优先
            glUniformMatrix4fv(mvpLoc, 1, GL_TRUE, cmd.mvpMatrix);
        }

        // 设置纹理采样器
        GLint texLoc = glGetUniformLocation(pipe->Program(), "uTex");
        if (texLoc != -1)
        {
            glUniform1i(texLoc, 0); // 纹理单元 0
        }

        glBindVertexArray(vao->Handle());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex->Handle());
        GLenum indexType = (cmd.indexType == IndexType::Uint16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        glDrawElements(GL_TRIANGLES, cmd.indexCount, indexType, nullptr);
    }
} // namespace ChikaEngine::Render