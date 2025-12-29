#pragma once
#include "render/meshdata.h"
#include "render/rhi/RHIResources.h"

#include <cstdint>

namespace ChikaEngine::Render
{
    class GLVertexArray final : public IRHIVertexArray
    {
      public:
        GLVertexArray(const MeshData& meshData);
        ~GLVertexArray() override;

        GLVertexArray(const GLVertexArray&) = delete;
        GLVertexArray& operator=(const GLVertexArray&) = delete;
        // NOTE:
        GLVertexArray(GLVertexArray&&) noexcept = delete;
        GLVertexArray& operator=(GLVertexArray&&) noexcept = default;

        void Bind() const;
        void UnBind() const;
        std::uint32_t IndexCount() const;

      private:
        unsigned int _VAO{0};
        unsigned int _VBO{0};
        unsigned int _EBO{0};
        std::uint32_t _indexCount{0};
    };
} // namespace ChikaEngine::Render