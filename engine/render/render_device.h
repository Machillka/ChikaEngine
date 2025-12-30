#pragma once

#include "math/mat4.h"
#include "render/camera.h"
#include "render/material.h"

#include <cstdint>
namespace ChikaEngine::Render
{
    struct RenderObject
    {
        const class IRHIVertexArray* VAO{nullptr};
        const class IRHIPipeline* pipeline{nullptr};
        const class Material* material{nullptr};
        const class Camera* camera{nullptr};
        Math::Mat4 modelMat = Math::Mat4::Identity;
    };

    // 对外暴露接口
    class IRenderDevice
    {
      public:
        virtual ~IRenderDevice() = default;
        virtual void Init() = 0;
        virtual void Shutdown() = 0;
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void DrawObject(const RenderObject& obj) = 0;
        virtual void OnResize(std::uint32_t width, std::uint32_t height) = 0;
    };

} // namespace ChikaEngine::Render