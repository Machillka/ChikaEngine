#pragma once
#include "color.h"

#include <memory>

namespace ChikaEngine::Render
{
    class IRHIShader;

    // TODO: 实现 PBR 材质
    class Material
    {
      public:
        explicit Material(std::shared_ptr<IRHIShader> shader) noexcept : _shader(shader) {}
        [[nodiscard]] std::shared_ptr<IRHIShader> Shader() const noexcept
        {
            return _shader;
        }
        void SetBaseColor(const Color& color)
        {
            _baseColor = color;
        }
        const Color& BaseColor() const noexcept
        {
            return _baseColor;
        }

      private:
        std::shared_ptr<IRHIShader> _shader;
        Color _baseColor{1.0f, 1.0f, 1.0f};
    };
} // namespace ChikaEngine::Render