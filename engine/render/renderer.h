#pragma once
#include "platform/window_system.h"
#include "render_api.h"
#include "render_device.h"

#include <memory>

namespace ChikaEngine::Render
{
    class Renderer
    {
      public:
        static void Init(RenderAPI api, ::ChikaEngine::Platform::IWindowSystem* window);
        static void Shutdown();

        // 渲染逻辑 全局单例
        static void BeginFrame();
        static void Submit(const RenderObject& obj);
        static void EndFrame();

      private:
        static inline std::unique_ptr<IRenderDevice> s_device{};
    };
} // namespace ChikaEngine::Render