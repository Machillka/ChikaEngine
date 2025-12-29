#pragma once
#include "math/mat4.h"
#include "math/vector3.h"
#include "render/rhi/RHIResources.h"
#include "render/shader_source.h"
namespace ChikaEngine::Render
{
    class GLShader final : public IRHIShader
    {
      public:
        explicit GLShader(const ShaderSource& src);
        ~GLShader() override;

        GLShader(const GLShader&) = delete;
        GLShader& operator=(const GLShader&) = delete;
        GLShader(GLShader&&) noexcept = default;
        GLShader& operator=(GLShader&&) noexcept = default;
        void Bind() const override;
        void Unbind() const override;
        void SetMat4(const char* name, const Math::Mat4& m) const override;
        void SetVec3(const char* name, const Math::Vector3& v) const override;

      private:
        unsigned int _programID{0};
        int GetUniformLocation(const char* name) const;
    };
} // namespace ChikaEngine::Render