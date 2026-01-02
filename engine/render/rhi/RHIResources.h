#pragma once
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
    };
} // namespace ChikaEngine::Render