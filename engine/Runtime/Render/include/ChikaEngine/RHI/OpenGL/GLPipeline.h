#pragma once

#include "ChikaEngine/RHI/RHIResources.h"
#include "GLHeader.h"
namespace ChikaEngine::Render
{
    class GLPipeline final : public IRHIPipeline
    {
      public:
        GLPipeline(const char* vsSource, const char* fsSource);
        ~GLPipeline() override;

        void Bind() override;

        void SetUniformFloat(const char* name, float v) override;
        void SetUniformVec3(const char* name, const std::array<float, 3>& v) override;
        void SetUniformVec4(const char* name, const std::array<float, 4>& v) override;
        void SetUniformMat4(const char* name, const Math::Mat4& v) override;

        void SetUniformTexture(const char* name, IRHITexture2D* tex, int slot) override;

        GLuint Program() const
        {
            return _program;
        }

      private:
        GLuint _program = 0;
        GLuint CompileShader(GLenum type, const char* src);
        GLint GetLocation(const char* name) const;
    };
} // namespace ChikaEngine::Render