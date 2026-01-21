#pragma once

#include "camera.h"
#include "render/renderobject.h"
namespace ChikaEngine::Render
{
    // 对外暴露接口
    class IRenderDevice
    {
      public:
        virtual ~IRenderDevice() = default;
        virtual void Init() = 0;
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void DrawObject(const RenderObject& obj, const Camera& camera) = 0;
        virtual void Shutdown() = 0;
    };

} // namespace ChikaEngine::Render