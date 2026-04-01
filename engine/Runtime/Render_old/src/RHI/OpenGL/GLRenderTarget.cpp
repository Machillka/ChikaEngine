

#include "ChikaEngine/RHI/OpenGL/GLRenderTarget.h"
#include "ChikaEngine/debug/assert.h"
namespace ChikaEngine::Render
{

    GLRenderTarget::GLRenderTarget(int width, int height) : _width(width), _height(height)
    {
        glGenFramebuffers(1, &_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

        _colorTex = new GLTexture2D(width, height, 4, nullptr, false);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTex->Handle(), 0);
        glGenRenderbuffers(1, &_depthRb);
        glBindRenderbuffer(GL_RENDERBUFFER, _depthRb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _depthRb);
        GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        CHIKA_ASSERT(status == GL_FRAMEBUFFER_COMPLETE, "GLRenderTarget framebuffer is incomplete");
        // 解绑，避免污染外部状态
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    GLRenderTarget::~GLRenderTarget()
    {
        if (_depthRb != 0)
        {
            glDeleteRenderbuffers(1, &_depthRb);
            _depthRb = 0;
        }
        if (_fbo != 0)
        {
            glDeleteFramebuffers(1, &_fbo);
            _fbo = 0;
        }
        delete _colorTex;
        _colorTex = nullptr;
    }

    void GLRenderTarget::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        GLint cur = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &cur);
        // LOG_INFO("RT", "Bind() _fbo={} current={}", _fbo, cur);
        glViewport(0, 0, _width, _height);
    }

    void GLRenderTarget::UnBind()
    {
        // 重新绑定回默认窗口
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    IRHITexture2D* GLRenderTarget::GetColorTexture()
    {
        return _colorTex;
    }
} // namespace ChikaEngine::Render