
#include "ChikaEngine/RHI/OpenGL/GLTexture2D.h"
namespace ChikaEngine::Render
{
    GLTexture2D::GLTexture2D(int width, int height, int channels, const void* data, bool sRGB)
    {
        glGenTextures(1, &_tex);
        glBindTexture(GL_TEXTURE_2D, _tex);
        GLenum format = GL_RGB;
        GLenum internalFormat = GL_RGB8;
        if (channels == 1)
        {
            format = GL_RED;
            internalFormat = GL_R8;
        }
        else if (channels == 3)
        {
            format = GL_RGB;
            internalFormat = sRGB ? GL_SRGB8 : GL_RGB8;
        }
        else if (channels == 4)
        {
            format = GL_RGBA;
            internalFormat = sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        }
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data); // 单通道贴图需要 swizzle，否则采样会全黑
        if (channels == 1)
        {
            GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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
