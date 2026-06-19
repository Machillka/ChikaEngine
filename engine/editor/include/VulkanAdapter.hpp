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

#include "ChikaEngine/RHIResourceHandle.hpp"
#include <vulkan/vulkan.h>

namespace ChikaEngine::Render
{
    class IRHICommandList;
    class Renderer;
    class VulkanRHIDevice;
} // namespace ChikaEngine::Render

struct GLFWwindow;

namespace ChikaEngine::Editor
{
    class VulkanAdapter
    {
      public:
        /**
         * @brief 初始化 Editor UI 上下文和 Vulkan 后端，并向 Renderer 安装通用 Overlay 回调。
         *
         * Editor 显式依赖 Vulkan Backend 类型，避免把 UI SDK 类型泄漏进通用 Render/RHI API。
         */
        void Initialize(GLFWwindow* window, Render::Renderer* renderer);
        /** @brief 等待 GPU、释放 UI 纹理和后端资源，并移除 Renderer Overlay 回调。 */
        void Shutdown();

        /** @brief 开启新的一帧 Editor UI。 */
        void BeginFrame();
        /** @brief 完成 UI 布局并生成 DrawData，实际绘制由 Overlay Pass 录制。 */
        void EndFrame();
        /** @brief 将 Runtime Texture 映射为 Editor UI 使用的 Vulkan Descriptor Set。 */
        void* GetTextureHandle(Render::TextureHandle texture);

      private:
        /** @brief 在通用 Overlay Pass 中使用 Vulkan Command Buffer 录制 Editor UI。 */
        void RenderOverlay(Render::IRHICommandList* commandList);
        /** @brief 释放当前 Viewport Texture Descriptor；调用前确保 GPU 不再使用它。 */
        void ReleaseTextureDescriptor();

        Render::Renderer* _renderer = nullptr;
        Render::VulkanRHIDevice* _device = nullptr;
        VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
        Render::TextureHandle _registeredTexture;
        VkDescriptorSet _registeredTextureDescriptor = VK_NULL_HANDLE;
        bool _contextInitialized = false;
        bool _glfwBackendInitialized = false;
        bool _vulkanBackendInitialized = false;
    };
} // namespace ChikaEngine::Editor
