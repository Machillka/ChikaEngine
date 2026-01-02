#include "GLVertexArray.h"

namespace ChikaEngine::Render
{
    GLVertexArray::GLVertexArray()
    {
        glGenVertexArrays(1, &m_vao);
    }

    GLVertexArray::~GLVertexArray()
    {
        glDeleteVertexArrays(1, &m_vao);
    }
} // namespace ChikaEngine::Render
