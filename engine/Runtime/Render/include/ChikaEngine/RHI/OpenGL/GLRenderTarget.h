#pragma once

#include "ChikaEngine/RHI/OpenGL/GLTexture2D.h"
#include "ChikaEngine/RHI/RHIResources.h"
#include "glfw/src/internal.h"
namespace ChikaEngine::Render
{

    class GLRenderTarget : public IRHIRenderTarget
    {
      public:
        GLRenderTarget(int width, int height);
        ~GLRenderTarget();

        void Bind() override;
        void UnBind() override;
        IRHITexture2D* GetColorTexture() override;

        int GetHeight() const override
        {
            return _height;
        }

        int GetWidth() const override
        {
            return _width;
        }

      private:
        int _height;
        int _width;
        GLuint _fbo = 0;
        GLuint _depthRb = 0;
        GLTexture2D* _colorTex;
    };
} // namespace ChikaEngine::Render