#pragma once

#include "render/rhi/RHIResources.h"

#include <memory>
namespace ChikaEngine::Render
{
    class GLPipeline final : public IRHIPipeline
    {
      public:
        explicit GLPipeline(std::shared_ptr<IRHIShader> shader) noexcept;

        void Bind() const override;
        void UnBind() const override;

        [[nodiscard]] std::shared_ptr<IRHIShader> GetShader() const noexcept;

      private:
        std::shared_ptr<IRHIShader> _shader;
    };
} // namespace ChikaEngine::Render