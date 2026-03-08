#pragma once
#include "Gizmo.h"
#include "RHI/RHIDevice.h"
#include "ChikaEngine/CameraData.h"
#include <memory>

namespace ChikaEngine::Render
{
    class GizmoRenderer
    {
      public:
        void Init(IRHIDevice* rhiDevice);
        void Shutdown();

        // 将收集到的所有线框绘制到目标 RenderTarget
        void Render(IRHIRenderTarget* target, const CameraData& camera, const std::vector<GizmoVertex>& lines);

      private:
        // 只是借用
        IRHIDevice* _rhiDevice = nullptr;
        std::unique_ptr<IRHIVertexArray> _vao;
        std::unique_ptr<IRHIBuffer> _vbo;
        std::unique_ptr<IRHIPipeline> _pipeline;

        static constexpr size_t MAX_VERTICES = 20000;
    };
} // namespace ChikaEngine::Render