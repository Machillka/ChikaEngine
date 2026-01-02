#pragma once

#include "RHITypes.h"

#include <cstddef>
#include <cstdint>
namespace ChikaEngine::Render
{

    // 资源声明
    class IRHIVertexArray;
    class IRHIBuffer;
    class IRHITexture2D;
    class IRHIPipeline;

    // 将绘制命令进行抽象，方便拓展
    struct DrawIndexedCommand
    {
        const IRHIVertexArray* vao = nullptr;
        const IRHIPipeline* pipe = nullptr;
        const IRHITexture2D* tex = nullptr;
        uint32_t indexCount = 0;
        IndexType indexType = IndexType::Uint32;
        const float* mvpMatrix = nullptr;  // 4x4 MVP矩阵数据
    };

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
        virtual IRHITexture2D* CreateTexture2D(int w, int h, const void* data) = 0;
        virtual IRHIPipeline* CreatePipeline(const char* vsSource, const char* fsSource) = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual void DrawIndexed(const DrawIndexedCommand& cmd) = 0;
    };
} // namespace ChikaEngine::Render