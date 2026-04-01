// GLTexture2D.h
#pragma once
#include "ChikaEngine/RHI/RHIResources.h"
#include "GLHeader.h"

namespace ChikaEngine::Render
{
    class GLTexture2D : public IRHITexture2D
    {
      public:
        GLTexture2D(int width, int height, int channels, const void* data, bool sRGB);
        ~GLTexture2D() override;

        std::uintptr_t Handle() const override;

      private:
        GLuint _tex = 0;
    };
} // namespace ChikaEngine::Render
