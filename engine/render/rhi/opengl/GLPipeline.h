#pragma once

#include "GLHeader.h"
#include "render/rhi/RHIResources.h"
namespace ChikaEngine::Render
{
    class GLPipeline final : public IRHIPipeline
    {
      public:
        GLPipeline(const char* vsSource, const char* fsSource);
        ~GLPipeline() override;
        GLuint Program() const
        {
            return _program;
        }

      private:
        GLuint _program = 0;
        GLuint CompileShader(GLenum type, const char* src);
    };
} // namespace ChikaEngine::Render