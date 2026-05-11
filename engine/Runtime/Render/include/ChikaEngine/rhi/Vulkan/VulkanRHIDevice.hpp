#pragma once

#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RHIResourceHandle.hpp"
#include "ChikaEngine/base/SlotMap.h"
#include "ChikaEngine/rhi/Vulkan/VulkanResource.hpp"
#include "VulkanResource.hpp"
#include <cstdint>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>

namespace ChikaEngine::Render
{

    class VulkanRHIDevice final : public IRHIDevice
    {
      public:
        VulkanRHIDevice(const RHI_InitParams& params);
        ~VulkanRHIDevice() override;

        void Initialize(const RHI_InitParams& params) override;
        void Shutdown() override;
        void BeginFrame() override;
        void EndFrame() override;
        void Submit(IRHICommandList* cmdList) override;

        BufferHandle CreateBuffer(const BufferDesc& desc) override;
        TextureHandle CreateTexture(const TextureDesc& desc) override;
        ShaderHandle CreateShader(const ShaderDesc& desc) override;
        PipelineHandle CreateGraphicsPipeline(const PipelineDesc& desc) override;

        void* GetMappedData(BufferHandle handle) override;

        IRHICommandList* AllocateCommandList() override;
        void DestroyBuffer(BufferHandle handle) override;
        void DestroyTexture(TextureHandle handle) override;

        void* GetImGuiTextureHandle(TextureHandle handle) override;
        TextureHandle GetActiveSwapchainTexture() override;

        void InitializeImgui() override;

        void WaitIdle() override;

        void Resize(uint32_t width, uint32_t height) override;

      public:
        VkDescriptorSet AllocateTransientDescriptorSet();
        VulkanBuffer* GetVkBuffer(BufferHandle h)
        {
            return m_buffers.Get(h);
        }
        VulkanTexture* GetVkTexture(TextureHandle h)
        {
            return m_textures.Get(h);
        }
        VulkanPipeline* GetVkPipeline(PipelineHandle h)
        {
            return m_pipelines.Get(h);
        }
        VkDevice GetRawDevice() const
        {
            return m_device;
        }
        VkDescriptorSetLayout GetGlobalDescriptorLayout() const
        {
            return m_globalDescriptorLayout;
        }
        VkSampler GetDefaultSampler() const
        {
            return m_defaultSampler;
        }

      public:
        VkDescriptorPool imguiPool = VK_NULL_HANDLE;

      private:
        void CreateInstance();
        void CreateSurface();
        void PickPhysicalDevice();
        void CreateCommandPools();
        void CreateLogicalDevice();
        void CreateAllocator();
        void CreateGlobalLayouts();
        void CreateSwapchain();
        void CreateSyncObjects();
        void FlushDeletionQueue();
        void CleanupSwapchain();

      private:
        // 跳过失败帧
        bool m_frameSkipped = false;

      private:
        uint32_t m_height;
        uint32_t m_width;

        uint32_t m_imageCount;
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        VkInstance m_instance = VK_NULL_HANDLE;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        uint32_t m_graphicsQueueFamily = 0;

        VmaAllocator m_allocator = VK_NULL_HANDLE;

        VkDescriptorSetLayout m_globalDescriptorLayout = VK_NULL_HANDLE;
        VkSampler m_defaultSampler = VK_NULL_HANDLE;

        uint32_t m_currentFrame = 0;
        uint64_t m_absoluteFrame = 0;
        VkCommandPool m_commandPools[MAX_FRAMES_IN_FLIGHT];
        VkDescriptorPool m_descriptorPools[MAX_FRAMES_IN_FLIGHT];
        // VkFence m_inFlightFences[MAX_FRAMES_IN_FLIGHT];
        std::vector<VkFence> m_inFlightFences;

        // submit 的时候统一提交
        std::vector<VkCommandBuffer> m_pendingCmds[MAX_FRAMES_IN_FLIGHT];

        // swapchain surface同步原语以及其他
        void* m_windowHandle = nullptr;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
        VkFormat m_swapchainFormat;
        VkExtent2D m_swapchainExtent;

        // VkImage m_swapchainImages[2];
        // VkImageView m_swapchainImageViews[2];
        std::vector<VkImage> m_swapchainImages;
        std::vector<VkImageView> m_swapchainImageViews;
        // TextureHandle m_swapchainTextures[MAX_FRAMES_IN_FLIGHT];
        std::vector<TextureHandle> m_swapchainTextures; // 将 Swapchain 图包装为 Texture 符合设计

        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        // VkSemaphore m_imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
        // VkSemaphore m_renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
        uint32_t m_currentImageIndex = 0;

      private:
        // 资源缓存 SlotMap
        // TODO[x]: 使用 record 来管理内存空间
        Core::SlotMap<BufferHandle, VulkanBuffer> m_buffers;
        Core::SlotMap<TextureHandle, VulkanTexture> m_textures;
        Core::SlotMap<ShaderHandle, VulkanShader> m_shaders;
        Core::SlotMap<PipelineHandle, VulkanPipeline> m_pipelines;

        struct PendingDeletion
        {
            uint64_t frameIndex;
            uint32_t handleRaw;
        };
        // 延迟删除
        // TODO: 使用自定义的线程安全队列实现
        std::mutex m_deletionMutex;
        std::vector<PendingDeletion> m_bufferDeletionQueue;
        std::vector<PendingDeletion> m_textureDeletionQueue;
    };
} // namespace ChikaEngine::Render