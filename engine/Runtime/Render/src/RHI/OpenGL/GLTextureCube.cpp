#include "ChikaEngine/RHI/OpenGL/GLTextureCube.h"
namespace ChikaEngine::Render
{

    GLTextureCube::GLTextureCube(int width, int height, int channels, const std::array<const void*, 6>& facesData, bool sRGB)
    {
        glGenTextures(1, &_textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _textureID);

        GLenum internalFormat = sRGB ? (channels == 4 ? GL_SRGB_ALPHA : GL_SRGB) : (channels == 4 ? GL_RGBA : GL_RGB);
        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

        for (unsigned int i = 0; i < 6; i++)
        {
            if (facesData[i])
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, facesData[i]);
            }
        }

        // 天空盒的纹理过滤与环绕方式
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    GLTextureCube::~GLTextureCube()
    {
        glDeleteTextures(1, &_textureID);
    }

    void* GLTextureCube::Handle() const
    {
        return (void*)(uintptr_t)_textureID;
    }
} // namespace ChikaEngine::Render