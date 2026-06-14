#pragma once

#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RenderDiagnostics.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RHIResourceHandle.hpp"
#include "ChikaEngine/base/SlotMap.h"
#include "ChikaEngine/rhi/Vulkan/VulkanResource.hpp"
#include "VulkanResource.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <unordered_map>
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
        SamplerHandle CreateSampler(const SamplerDesc& desc) override;
        TextureViewHandle CreateTextureView(const TextureViewDesc& desc) override;
        ShaderHandle CreateShader(const ShaderDesc& desc) override;
        PipelineHandle CreateGraphicsPipeline(const PipelineDesc& desc) override;
        PipelineHandle CreateComputePipeline(const ComputePipelineDesc& desc) override;

        void* GetMappedData(BufferHandle handle) override;

        void SetDebugName(BufferHandle handle, std::string_view name) override;
        void SetDebugName(TextureHandle handle, std::string_view name) override;
        void SetDebugName(ShaderHandle handle, std::string_view name) override;
        void SetDebugName(PipelineHandle handle, std::string_view name) override;
        void SetDebugName(SamplerHandle handle, std::string_view name) override;
        void SetDebugName(TextureViewHandle handle, std::string_view name) override;
        const RenderFrameStatistics& GetFrameStatistics() const override
        {
            return m_frameStatistics;
        }
        const std::vector<RenderPassGpuTiming>& GetPassGpuTimings() const override
        {
            return m_passGpuTimings;
        }

        IRHICommandList* AllocateCommandList() override;
        void DestroyBuffer(BufferHandle handle) override;
        void DestroyTexture(TextureHandle handle) override;
        void DestroyShader(ShaderHandle handle) override;
        void DestroyPipeline(PipelineHandle handle) override;
        void DestroySampler(SamplerHandle handle) override;
        void DestroyTextureView(TextureViewHandle handle) override;

        void* GetImGuiTextureHandle(TextureHandle handle) override;
        TextureHandle GetActiveSwapchainTexture() override;

        void InitializeImgui() override;

        void WaitIdle() override;

        void Resize(uint32_t width, uint32_t height) override;

      public:
        struct DescriptorAllocation
        {
            VkDescriptorSet set = VK_NULL_HANDLE;
            bool requiresUpdate = true;
        };

        /**
         * @brief 按当前 Pipeline 的目标 Set Layout 分配或复用 Descriptor Set。
         */
        DescriptorAllocation AllocateDescriptorSet(const VulkanPipeline& pipeline, const ResourceBindingGroup& group);
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
        VkSampler GetDefaultSampler() const
        {
            return m_defaultSampler;
        }
        VkSampler GetVkSampler(SamplerHandle handle) const
        {
            const VulkanSampler* sampler = m_samplers.Get(handle);
            return sampler ? sampler->sampler : m_defaultSampler;
        }
        VkImageView GetVkTextureView(TextureViewHandle handle) const
        {
            const VulkanTextureView* view = m_textureViews.Get(handle);
            return view ? view->view : VK_NULL_HANDLE;
        }

      public:
        VkDescriptorPool imguiPool = VK_NULL_HANDLE;

      private:
        friend class VulkanCommandList;

        /**
         * @brief 创建 Vulkan Debug Messenger，把验证层消息转发到引擎日志系统。
         *
         * Messenger 仅在启用 Validation 时创建，避免 Release 路径承担额外诊断开销。
         */
        void CreateDebugMessenger();
        void DestroyDebugMessenger();

        /**
         * @brief 为任意 Vulkan 对象设置调试名称。
         *
         * 统一封装可避免各资源路径重复加载 Debug Utils 函数指针。
         */
        void SetVulkanObjectName(VkObjectType type, uint64_t objectHandle, std::string_view name);

        /**
         * @brief 记录一次显式 Draw，用于建立可比较的帧提交基线。
         */
        void RecordDraw(uint32_t instanceCount);

        /**
         * @brief 记录一次 Pipeline Bind，用于后续验证排序和批处理收益。
         */
        void RecordPipelineBind();

        /**
         * @brief 记录实际写入 Descriptor 的数量，而不是仅统计 Descriptor Set 数量。
         */
        void RecordDescriptorUpdates(uint32_t descriptorCount);
        void PrepareTimestampQueries(VkCommandBuffer commandBuffer);
        uint32_t AllocateTimestampScope(std::string_view name);

        void CreateInstance();
        void CreateSurface();
        void PickPhysicalDevice();
        void CreateCommandPools();
        void CreateLogicalDevice();
        void CreatePipelineCache();
        void SavePipelineCache();
        void CreateAllocator();

        /**
         * @brief 创建通用 Descriptor Pools 与默认 Sampler，具体 Layout 由 Reflection 按需生成。
         */
        void CreateDescriptorInfrastructure();

        /**
         * @brief 按 Reflection Binding 创建或复用 Descriptor Set Layout。
         */
        VkDescriptorSetLayout GetOrCreateDescriptorSetLayout(std::span<const Shader::ShaderResourceBinding> bindings);

        /**
         * @brief 按合并 Shader Interface 创建或复用 Pipeline Layout。
         */
        VulkanPipeline CreatePipelineLayout(const Shader::ShaderProgramInterface& interface);
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
        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        uint32_t m_graphicsQueueFamily = 0;
        VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
        std::filesystem::path m_pipelineCachePath;

        VmaAllocator m_allocator = VK_NULL_HANDLE;

        VkSampler m_defaultSampler = VK_NULL_HANDLE;
        bool m_enableValidation = true;
        RenderFrameStatistics m_frameStatistics;
        std::vector<RenderPassGpuTiming> m_passGpuTimings;

        uint32_t m_currentFrame = 0;
        uint64_t m_absoluteFrame = 0;
        VkCommandPool m_commandPools[MAX_FRAMES_IN_FLIGHT];
        VkDescriptorPool m_descriptorPools[MAX_FRAMES_IN_FLIGHT];
        VkQueryPool m_timestampQueryPools[MAX_FRAMES_IN_FLIGHT]{};
        bool m_timestampQueriesReset = false;
        static constexpr uint32_t MAX_TIMESTAMP_QUERIES = 512;
        struct TimestampScope
        {
            std::string name;
            uint32_t beginQuery = 0;
            uint32_t endQuery = 0;
        };
        std::vector<TimestampScope> m_timestampScopes[MAX_FRAMES_IN_FLIGHT];
        float m_timestampPeriodNs = 1.0f;
        VkDescriptorPool m_persistentDescriptorPool = VK_NULL_HANDLE;
        std::unordered_map<uint64_t, VkDescriptorSetLayout> m_descriptorSetLayoutCache;
        std::unordered_map<uint64_t, VkPipelineLayout> m_pipelineLayoutCache;
        std::unordered_map<uint64_t, VkDescriptorSet> m_persistentDescriptorSetCache;
        // VkFence m_inFlightFences[MAX_FRAMES_IN_FLIGHT];
        std::vector<VkFence> m_inFlightFences;

        // submit 的时候统一提交
        std::vector<VkCommandBuffer> m_pendingCmds[MAX_FRAMES_IN_FLIGHT];
        std::vector<std::unique_ptr<IRHICommandList>> m_submittedCommandLists[MAX_FRAMES_IN_FLIGHT];

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
        Core::SlotMap<SamplerHandle, VulkanSampler> m_samplers;
        Core::SlotMap<TextureViewHandle, VulkanTextureView> m_textureViews;

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
        std::vector<PendingDeletion> m_shaderDeletionQueue;
        std::vector<PendingDeletion> m_pipelineDeletionQueue;
        std::vector<PendingDeletion> m_samplerDeletionQueue;
        std::vector<PendingDeletion> m_textureViewDeletionQueue;
    };
} // namespace ChikaEngine::Render
