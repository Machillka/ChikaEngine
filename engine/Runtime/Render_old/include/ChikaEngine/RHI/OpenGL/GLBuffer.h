#pragma once
#include "ChikaEngine/RHI/RHIResources.h"
#include "GLHeader.h"

#include <cstddef>
namespace ChikaEngine::Render
{
    enum class GLBufferType
    {
        Vertex,
        Index
    };

    class GLBuffer : public IRHIBuffer
    {
      public:
        GLBuffer(GLBufferType type, std::size_t size, const void* data);
        ~GLBuffer() override;
        GLuint Handle() const
        {
            return _buffer;
        }
        GLBufferType Type() const
        {
            return m_type;
        }

      private:
        GLuint _buffer = 0;
        GLBufferType m_type;
    };
} // namespace ChikaEngine::Render