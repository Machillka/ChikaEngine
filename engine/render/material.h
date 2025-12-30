#pragma once
#include "color.h"
#include "math/vector3.h"

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
        void SetBaseColor(const Math::Vector3& color)
        {
            _baseColor = color;
        }
        const Math::Vector3& BaseColor() const noexcept
        {
            return _baseColor;
        }

      private:
        std::shared_ptr<IRHIShader> _shader;
        Math::Vector3 _baseColor{1.0f, 1.0f, 1.0f};
    };
} // namespace ChikaEngine::Render