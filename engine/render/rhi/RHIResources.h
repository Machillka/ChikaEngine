#pragma once
#include <cstdint>
namespace ChikaEngine::Render
{
    // 顶点缓冲区
    class IRHIBuffer
    {
      public:
        virtual ~IRHIBuffer() = default;
    };

    class IRHIVertexArray
    {
      public:
        virtual ~IRHIVertexArray() = default;
    };

    class IRHIShader
    {
      public:
        virtual ~IRHIShader() = default;
    };

    // 渲染管线抽象
    class IRHIPipeline
    {
      public:
        virtual ~IRHIPipeline() = default;
    };

    class IRHITexture2D
    {
      public:
        virtual ~IRHITexture2D() = default;
        virtual std::uintptr_t Handle() const = 0;
    };

    // 定义渲染输出的对象
    class IRHIRenderTarget
    {
      public:
        virtual ~IRHIRenderTarget() = default;
        virtual void Bind() = 0;
        virtual void UnBind() = 0;
        // 返回渲染结果
        virtual IRHITexture2D* GetColorTexture() = 0;

        virtual int GetHeight() const = 0;
        virtual int GetWidth() const = 0;
    };
} // namespace ChikaEngine::Render