#include "render/render_device.h"
#include "render/rhi/RHIDevice.h"

namespace ChikaEngine::Render
{

    class GLRenderDevice : public IRenderDevice
    {
      public:
        explicit GLRenderDevice(IRHIDevice* device);
        ~GLRenderDevice() override;
        void Init() override;
        void BeginFrame() override;
        void EndFrame() override;
        void DrawObject(const RenderObject& obj, const Math::Mat4& view, const Math::Mat4& proj) override;
        void Shutdown() override;

      private:
        IRHIDevice* _glRHIDevice = nullptr;
    };
} // namespace ChikaEngine::Render