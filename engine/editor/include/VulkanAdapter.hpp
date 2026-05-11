/*!
 * @file VulkanAdapter.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 图形渲染后端不应该知道 Imgui 的存在, 所以在 Editor 层重新写一个 Adapter
 * @version 0.1
 * @date 2026-05-05
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

namespace ChikaEngine::Render
{
    class Renderer;
}

struct GLFWwindow;

namespace ChikaEngine::Editor
{
    class VulkanAdapter
    {
      public:
        // 初始化 ImGui 上下文，对接 GLFW，并调用 RHI 的 ImGui 初始化
        void Initialize(GLFWwindow* window, Render::Renderer* renderer);
        void Shutdown();

        // 开启新的一帧 ImGui
        void BeginFrame();
        // 结束 ImGui 逻辑提交，生成 DrawData，并处理多窗口 Viewport
        void EndFrame();

      private:
        Render::Renderer* _renderer = nullptr;
    };
} // namespace ChikaEngine::Editor