#pragma once

#include "ChikaEngine/RHI/RHIResources.h"
#include <array>
#include "GLHeader.h"

namespace ChikaEngine::Render
{
    class GLTextureCube : public IRHITextureCube
    {
      public:
        GLTextureCube(int width, int height, int channels, const std::array<const void*, 6>& facesData, bool sRGB = false);
        ~GLTextureCube() override;
        void* Handle() const override;

      private:
        GLuint _textureID;
    };
} // namespace ChikaEngine::Render