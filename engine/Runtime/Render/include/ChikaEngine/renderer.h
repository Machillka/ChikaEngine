#pragma once
#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/render_api.h"
#include "ChikaEngine/render_device.h"
#include "ChikaEngine/renderobject.h"
#include <memory>

namespace ChikaEngine::Render
{
    enum class RenderObjectPrefabs
    {
        Cube
    };
    class Renderer
    {
      public:
        static void Init(RenderAPI api);
        static void Shutdown();

        // 渲染逻辑 全局单例 初始化依赖注入之后 只需要全局调用一份
        // TODO: 取消全局单例 改成服务器 以支持多窗口
        static void BeginFrame();
        // 渲染scene中所有物体
        // TODO: 加入scene
        static void RenderObjects(const std::vector<RenderObject>& ros, const CameraData& camera);
        static void RenderObjectsToTarget(IRHIRenderTarget* target, const std::vector<RenderObject>& ros, const CameraData& camera);
        static IRHIRenderTarget* CreateRenderTarget(int width, int height);

        static void RenderObjectsToTargetWithSkyBox(IRHIRenderTarget* target, TextureCubeHandle cubemap, const std::vector<RenderObject>& ros, const CameraData& camera);
        static void DrawSkybox(TextureCubeHandle cubemap, const CameraData& camera);
        static void EndFrame();

      private:
        static inline std::unique_ptr<IRenderDevice> _renderDevice{};
        static inline std::unique_ptr<IRHIDevice> _rhiDevice{};
    };
} // namespace ChikaEngine::Render