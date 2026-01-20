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

        std::uintptr_t Handle() const override;

      private:
        GLuint _tex = 0;
    };
} // namespace ChikaEngine::Render
