#pragma once
#include "render/rhi/RHIDevice.h"

namespace ChikaEngine::Render
{
    class GLRHIDevice : public IRHIDevice
    {
      public:
        ~GLRHIDevice();
        void BeginFrame() override;
        void EndFrame() override;
        IRHIVertexArray* CreateVertexArray() override;
        IRHIBuffer* CreateVertexBuffer(std::size_t size, const void* data) override;
        IRHIBuffer* CreateIndexBuffer(std::size_t size, const void* data) override;
        IRHITexture2D* CreateTexture2D(int width, int height, const void* data) override;
        IRHIPipeline* CreatePipeline(const char* vsSource, const char* fsSource) override;
        IRHIRenderTarget* CreateRenderTarget(int width, int height) override;
        void DrawIndexed(const DrawIndexedCommand& cmd) override;
    };
} // namespace ChikaEngine::Render