/*!
 * @file VKRHIDevice.h
 * @brief Vulkan RHI 设备 - 完整的渲染硬件接口实现
 * @date 2026-03-07
 *
 * 核心职责：
 * 1. 管理 Vulkan 实例、物理设备、逻辑设备
 * 2. 创建和管理所有 RHI 资源（缓冲区、纹理、管线等）
 * 3. 提供统一的资源管理接口
 * 4. 处理命令缓冲区的记录和提交
 */
#pragma once

#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/RHI/RHITypes.h"
#include "VKCommon.h"
#include "VKResources.h"
#include <memory>
#include <vector>

namespace ChikaEngine::Render::Vulkan
{
    /// ========== Vulkan RHI 设备实现 ==========
    class VKRHIDevice : public IRHIDevice
    {
      public:
        VKRHIDevice();
        ~VKRHIDevice();

        /// ========== 设备初始化/销毁 ==========
        /// 初始化 Vulkan 设备（需要窗口句柄用于呈现）
        void Initialize(void* windowHandle, int windowWidth, int windowHeight, bool enableValidation = false);

        /// 清理 Vulkan 设备和所有资源
        void Shutdown();

        /// 调整窗口大小（重建交换链）
        void OnResize(int width, int height);

        /// ========== RHI 接口实现 - 资源创建 ==========
        IRHIVertexArray* CreateVertexArray() override;
        IRHIBuffer* CreateVertexBuffer(std::size_t size, const void* data) override;
        IRHIBuffer* CreateIndexBuffer(std::size_t size, const void* data) override;
        IRHITexture2D* CreateTexture2D(int width, int height, int channels, const void* data, bool sRGB) override;
        IRHIPipeline* CreatePipeline(const char* vsSource, const char* fsSource) override;
        IRHIRenderTarget* CreateRenderTarget(int width, int height) override;
        IRHITextureCube* CreateTextureCube(int w, int h, int channels, const std::array<const void*, 6>& data, bool sRGB) override;

        /// ========== RHI 接口实现 - 顶点布局设置 ==========
        void SetupGizmoVertexLayout(IRHIVertexArray* vao, IRHIBuffer* vbo) override;
        void SetupMeshVertexLayout(IRHIVertexArray* vao, IRHIBuffer* vbo, IRHIBuffer* ibo) override;

        /// ========== RHI 接口实现 - 渲染命令 ==========
        void BeginFrame() override;
        void EndFrame() override;
        void DrawIndexed(const RHIMesh& mesh) override;
        void UpdateBufferData(IRHIBuffer* buffer, std::size_t size, const void* data, std::size_t offset = 0) override;
        void DrawLines(IRHIVertexArray* vao, std::uint32_t vertexCount, std::uint32_t firstVertex = 0) override;

        /// ========== Vulkan 特定接口 ==========
        VkDevice GetVkDevice() const
        {
            return device_;
        }
        VkPhysicalDevice GetPhysicalDevice() const
        {
            return physicalDevice_;
        }
        VkQueue GetGraphicsQueue() const
        {
            return graphicsQueue_;
        }
        VkCommandPool GetCommandPool() const
        {
            return commandPool_;
        }
        VkRenderPass GetDefaultRenderPass() const
        {
            return defaultRenderPass_;
        }
        VkExtent2D GetSwapChainExtent() const
        {
            return swapChainExtent_;
        }

        /// 开始渲染通道
        void BeginRenderPass(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer, VkRenderPass renderPass, const VkClearValue& clearColor);

        /// 结束渲染通道
        void EndRenderPass(VkCommandBuffer cmdBuffer);

        /// 绑定管线进行绘制
        void BindPipeline(VkCommandBuffer cmdBuffer, VkPipeline pipeline);

      private:
        /// ========== Vulkan 核心对象 ==========
        VkInstance instance_ = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;

        /// ========== 队列相关 ==========
        VkQueue graphicsQueue_ = VK_NULL_HANDLE;
        VkQueue presentQueue_ = VK_NULL_HANDLE;
        QueueFamilyIndices queueFamilyIndices_;

        /// ========== 命令缓冲区 ==========
        VkCommandPool commandPool_ = VK_NULL_HANDLE;
        VkCommandBuffer currentCommandBuffer_ = VK_NULL_HANDLE;

        /// ========== 交换链 ==========
        VkSwapchainKHR swapChain_ = VK_NULL_HANDLE;
        std::vector<VkImage> swapChainImages_;
        std::vector<VkImageView> swapChainImageViews_;
        std::vector<VkFramebuffer> swapChainFramebuffers_;
        VkFormat swapChainImageFormat_ = VK_FORMAT_UNDEFINED;
        VkExtent2D swapChainExtent_{};

        /// ========== 同步原语 ==========
        VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
        VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
        VkFence inFlightFence_ = VK_NULL_HANDLE;
        uint32_t currentImageIndex_ = 0;

        /// ========== 渲染通道和管线 ==========
        VkRenderPass defaultRenderPass_ = VK_NULL_HANDLE;
        std::vector<std::unique_ptr<VKPipeline>> pipelines_;

        /// ========== 资源池 ==========
        std::vector<std::unique_ptr<VKBuffer>> buffers_;
        std::vector<std::unique_ptr<VKVertexArray>> vertexArrays_;
        std::vector<std::unique_ptr<VKTexture2D>> textures2D_;
        std::vector<std::unique_ptr<VKTextureCube>> texturesCube_;
        std::vector<std::unique_ptr<VKRenderTarget>> renderTargets_;

        /// ========== 调试支持 ==========
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;

        /// ========== 初始化辅助函数 ==========
        void CreateInstance(bool enableValidation);
        void SetupDebugMessenger(bool enableValidation);
        void CreateSurface(void* windowHandle);
        void SelectPhysicalDevice();
        void CreateLogicalDevice();
        void CreateCommandPool();
        void CreateSwapChain(int width, int height);
        void CreateImageViews();
        void CreateRenderPass();
        void CreateFramebuffers();
        void CreateSyncObjects();

        /// ========== 清理辅助函数 ==========
        void DestroySwapChain();
        void DestroySyncObjects();
        void DestroyDebugMessenger(bool hadDebugMessenger);

        /// ========== 队列族查询 ==========
        QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
        SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

        /// ========== 设备选择辅助 ==========
        bool IsDeviceSuitable(VkPhysicalDevice device);
        bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

        /// ========== 内存管理 ==========
        uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    };

} // namespace ChikaEngine::Render::Vulkan

// 为了兼容现有 OpenGL 代码，提供一个桥接
namespace ChikaEngine::Render
{
    using VKRHIDevice = Vulkan::VKRHIDevice;
}