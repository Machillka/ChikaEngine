#pragma once

#include "RHIResources.h"
#include "render/meshdata.h"
#include "render/shader_source.h"

#include <cstdint>
#include <memory>

namespace ChikaEngine::Render
{
    class IRHIDevice
    {
      public:
        virtual ~IRHIDevice() = default;
        virtual void Init() = 0;
        virtual void Shutdown() = 0;
        virtual void OnResize(std::uint32_t width, std::uint32_t height) = 0;
        virtual std::shared_ptr<IRHIVertexArray> CreateVertexArray(const MeshData& mesh) = 0;
        virtual std::shared_ptr<IRHIShader> CreateShader(const ShaderSource& source) = 0;
        // TODO:
        virtual std::shared_ptr<IRHIPipeline> CreatePipeline(std::shared_ptr<IRHIShader> shader) = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void DrawVertexArray() = 0;
    };
} // namespace ChikaEngine::Render