
#include "ChikaEngine/RHI/OpenGL/GLBuffer.h"
namespace ChikaEngine::Render
{
    GLBuffer::GLBuffer(GLBufferType type, std::size_t size, const void* data) : m_type(type)
    {
        glGenBuffers(1, &_buffer);
        GLenum target = (type == GLBufferType::Vertex) ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;
        glBindBuffer(target, _buffer);
        glBufferData(target, size, data, GL_STATIC_DRAW);
        glBindBuffer(target, 0);
    }

    GLBuffer::~GLBuffer()
    {
        glDeleteBuffers(1, &_buffer);
    }
} // namespace ChikaEngine::Render
