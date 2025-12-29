#include "GLVertexArray.h"

#include "glheader.h"
#include "render/meshdata.h"

#include <cstdint>

namespace ChikaEngine::Render
{

    GLVertexArray::GLVertexArray(const MeshData& meshData)
    {
        _indexCount = static_cast<std::uint32_t>(meshData.indices.size());

        glGenVertexArrays(1, &_VAO);
        glGenBuffers(1, &_VBO);
        glGenBuffers(1, &_EBO);

        glBindVertexArray(_VAO);

        glBindBuffer(GL_ARRAY_BUFFER, _VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(meshData.vertices.size() * sizeof(Vertex)),
                     meshData.vertices.data(),
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(meshData.indices.size() * sizeof(std::uint32_t)),
                     meshData.indices.data(),
                     GL_STATIC_DRAW);
        // Opengl Layout(location = 0) position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

        // Opengl Layout(location = 1) normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));

        // Opengl Layout(location = 2) uv
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));

        glBindVertexArray(0);
    }

    GLVertexArray::~GLVertexArray()
    {
        if (_EBO)
            glDeleteBuffers(1, &_EBO);
        if (_VBO)
            glDeleteBuffers(1, &_VBO);
        if (_VAO)
            glDeleteVertexArrays(1, &_VAO);
    }

    void GLVertexArray::Bind() const
    {
        glBindVertexArray(_VAO);
    }

    void GLVertexArray::UnBind() const
    {
        glBindVertexArray(0);
    }
} // namespace ChikaEngine::Render