#include "render/camera.h"
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
        void DrawObject(const RenderObject& obj, const Camera& camera) override;
        void Shutdown() override;

      private:
        IRHIDevice* _glRHIDevice = nullptr;
    };
} // namespace ChikaEngine::Render