#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/render_device.h"
#include "ChikaEngine/renderobject.h"

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
        void DrawObject(const RenderObject& obj, const CameraData& camera) override;
        void Shutdown() override;

      private:
        IRHIDevice* _glRHIDevice = nullptr;
    };
} // namespace ChikaEngine::Render