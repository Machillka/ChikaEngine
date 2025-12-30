#include "GLPipeline.h"

#include <memory>

namespace ChikaEngine::Render
{
    GLPipeline::GLPipeline(std::shared_ptr<IRHIShader> shader) noexcept : _shader(std::move(shader)) {}
    void GLPipeline::Bind() const
    {
        _shader->Bind();
    }
    void GLPipeline::UnBind() const
    {
        _shader->Unbind();
    }
    std::shared_ptr<IRHIShader> GLPipeline::GetShader() const noexcept
    {
        return _shader;
    }

} // namespace ChikaEngine::Render