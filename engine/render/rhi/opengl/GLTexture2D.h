// GLTexture2D.h
#pragma once
#include "GLHeader.h"
#include "render/rhi/RHIResources.h"

namespace ChikaEngine::Render
{
    class GLTexture2D : public IRHITexture2D
    {
      public:
        GLTexture2D(int width, int height, const void* data);
        ~GLTexture2D() override;

        GLuint Handle() const
        {
            return m_tex;
        }

      private:
        GLuint m_tex = 0;
    };
} // namespace ChikaEngine::Render
