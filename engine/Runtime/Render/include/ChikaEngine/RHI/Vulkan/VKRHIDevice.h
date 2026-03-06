#pragma once

#include "ChikaEngine/RHI/RHIDevice.h"
namespace ChikaEngine::Render
{
    class VKRHIDevice : public IRHIDevice
    {
      public:
        VKRHIDevice();
        ~VKRHIDevice();

        IRHIVertexArray* CreateVertexArray();
        IRHIBuffer* CreateVertexBuffer(std::size_t size, const void* data);
        IRHIBuffer* CreateIndexBuffer(std::size_t size, const void* data);
        IRHITexture2D* CreateTexture2D(int width, int height, int channels, const void* data, bool sRGB);
        IRHIPipeline* CreatePipeline(const char* vsSource, const char* fsSource);
        IRHIRenderTarget* CreateRenderTarget(int width, int height);
        IRHITextureCube* CreateTextureCube(int w, int h, int channels, const std::array<const void*, 6>& data, bool sRGB);

        void SetupGizmoVertexLayout(IRHIVertexArray* vao, IRHIBuffer* vbo);
        void SetupMeshVertexLayout(IRHIVertexArray* vao, IRHIBuffer* vbo, IRHIBuffer* ibo);

        void BeginFrame();
        void EndFrame();

        void DrawIndexed(const RHIMesh& mesh);
        void UpdateBufferData(IRHIBuffer* buffer, std::size_t size, const void* data, std::size_t offset);
        void DrawLines(IRHIVertexArray* vao, std::uint32_t vertexCount, std::uint32_t firstVertex = 0);
    };
} // namespace ChikaEngine::Render