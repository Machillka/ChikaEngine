#include "GLTexture2D.h"

namespace ChikaEngine::Render
{
    GLTexture2D::GLTexture2D(int width, int height, const void* data)
    {
        glGenTextures(1, &_tex);
        glBindTexture(GL_TEXTURE_2D, _tex);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLTexture2D::~GLTexture2D()
    {
        glDeleteTextures(1, &_tex);
    }
    std::uintptr_t GLTexture2D::Handle() const
    {
        return static_cast<uintptr_t>(_tex);
    }
} // namespace ChikaEngine::Render
