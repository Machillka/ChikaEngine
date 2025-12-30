#pragma once
#include "platform/window/window_system.h"
#include "render/render_device.h"
#include "render/rhi/RHIDevice.h"

namespace ChikaEngine::Render
{

    class GLDevice final : public IRenderDevice, public IRHIDevice
    {
      public:
        explicit GLDevice(::ChikaEngine::Platform::IWindowSystem& window) noexcept;
        ~GLDevice() override;
        void Init() override;
        void Shutdown() override;
        void BeginFrame() override;
        void EndFrame() override;
        void DrawObject(const RenderObject& obj) override;
        void OnResize(std::uint32_t width, std::uint32_t height) override;

        std::shared_ptr<IRHIVertexArray> CreateVertexArray(const MeshData& mesh) override;
        std::shared_ptr<IRHIShader> CreateShader(const ShaderSource& source) override;
        std::shared_ptr<IRHIPipeline> CreatePipeline(std::shared_ptr<IRHIShader> shader) override;
        void DrawVertexArray(const IRHIVertexArray& VAO, std::uint32_t indexCount) override;

      private:
        ::ChikaEngine::Platform::IWindowSystem& _window;
    };
} // namespace ChikaEngine::Render