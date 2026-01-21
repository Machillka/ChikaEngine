#pragma once
#include "glheader.h"
#include "render/rhi/RHIResources.h"
namespace ChikaEngine::Render
{
    class GLVertexArray : public IRHIVertexArray
    {
      public:
        GLVertexArray();
        ~GLVertexArray() override;

        GLuint Handle() const
        {
            return m_vao;
        }

      private:
        GLuint m_vao = 0;
    };
} // namespace ChikaEngine::Render
