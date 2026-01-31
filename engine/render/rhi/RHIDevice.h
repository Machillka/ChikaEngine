#pragma once

#include "RHIResources.h"
#include "render/Resource/Mesh.h"

#include <cstddef>

namespace ChikaEngine::Render
{

    // 资源声明
    class IRHIVertexArray;
    class IRHIBuffer;
    class IRHITexture2D;
    class IRHIPipeline;

    class IRHIDevice
    {
      public:
        virtual ~IRHIDevice() = default;
        // virtual void Init() = 0;
        // virtual void Shutdown() = 0;
        // virtual void OnResize(std::uint32_t width, std::uint32_t height) = 0;

        // 资源创建 使用缓冲池 资源交给上层释放 直接使用指针存储资源
        virtual IRHIVertexArray* CreateVertexArray() = 0;
        virtual IRHIBuffer* CreateVertexBuffer(std::size_t size, const void* data) = 0;
        virtual IRHIBuffer* CreateIndexBuffer(std::size_t size, const void* data) = 0;
        virtual IRHITexture2D* CreateTexture2D(int width, int height, int channels, const void* data, bool sRGB) = 0;
        virtual IRHIPipeline* CreatePipeline(const char* vsSource, const char* fsSource) = 0;
        virtual IRHIRenderTarget* CreateRenderTarget(int width, int height) = 0;

        // 绑定 vertex
        virtual void SetupMeshVertexLayout(IRHIVertexArray* vao, IRHIBuffer* vbo, IRHIBuffer* ibo) = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual void DrawIndexed(const RHIMesh& mesh) = 0;
    };
} // namespace ChikaEngine::Render