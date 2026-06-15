#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RHIResourceHandle.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/rhi/Vulkan/VulkanCommandList.hpp"
#include "ChikaEngine/rhi/Vulkan/VulkanResource.hpp"
#include <ChikaEngine/rhi/Vulkan/VulkanRHIDevice.hpp>
#include <ChikaEngine/rhi/Vulkan/VulkanHelper.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <GLFW/glfw3.h>
#include <vector>

namespace ChikaEngine::Render
{
    namespace
    {
        /**
         * @brief 把 Vulkan dispatchable 或 non-dispatchable handle 转换为 Debug Utils 所需的整数句柄。
         *
         * Vulkan 在不同平台可能把句柄声明为指针或整数，因此统一转换可以避免各命名函数重复平台判断。
         */
        template <typename T> uint64_t ToDebugObjectHandle(T handle)
        {
            if constexpr (std::is_pointer_v<T>)
                return reinterpret_cast<uint64_t>(handle);
            else
                return static_cast<uint64_t>(handle);
        }

        /**
         * @brief 组合稳定整数 Hash，用于 Vulkan Layout 和持久 Descriptor 缓存。
         */
        void HashCombine(uint64_t& hash, uint64_t value)
        {
            hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        }

        /**
         * @brief 计算单个 Descriptor Set Layout 的缓存 Key。
         */
        uint64_t HashDescriptorSetLayout(std::span<const Shader::ShaderResourceBinding> bindings)
        {
            uint64_t hash = 14695981039346656037ull;
            for (const auto& binding : bindings)
            {
                HashCombine(hash, binding.binding);
                HashCombine(hash, static_cast<uint32_t>(binding.type));
                HashCombine(hash, binding.arrayCount);
                HashCombine(hash, static_cast<uint32_t>(binding.stages));
            }
            return hash;
        }

        /**
         * @brief 计算持久 Descriptor Set 的资源内容 Key。
         */
        uint64_t HashResourceBindingGroup(const ResourceBindingGroup& group, VkDescriptorSetLayout layout)
        {
            uint64_t hash = ToDebugObjectHandle(layout);
            HashCombine(hash, group.set);
            for (const auto& buffer : group.buffers)
            {
                HashCombine(hash, buffer.binding);
                HashCombine(hash, buffer.arrayElement);
                HashCombine(hash, buffer.buf.raw_value);
                HashCombine(hash, buffer.offset);
                HashCombine(hash, buffer.size);
            }
            for (const auto& texture : group.textures)
            {
                HashCombine(hash, texture.binding);
                HashCombine(hash, texture.arrayElement);
                HashCombine(hash, texture.tex.raw_value);
                HashCombine(hash, texture.view.raw_value);
            }
            for (const auto& sampler : group.samplers)
            {
                HashCombine(hash, sampler.binding);
                HashCombine(hash, sampler.arrayElement);
                HashCombine(hash, sampler.sampler.raw_value);
            }
            return hash;
        }

        /**
         * @brief 返回颜色 Attachment 在 Shader 中对应的数值类型，用于 Pipeline 创建前接口校验。
         */
        Shader::ShaderValueType AttachmentValueType(RHI_Format format)
        {
            switch (format)
            {
            case RHI_Format::R32_Float:
                return Shader::ShaderValueType::Float;
            case RHI_Format::RG32_Float:
                return Shader::ShaderValueType::Float2;
            case RHI_Format::RGB32_Float:
                return Shader::ShaderValueType::Float3;
            case RHI_Format::RGBA32_SInt:
                return Shader::ShaderValueType::Int4;
            case RHI_Format::RGBA8_UNorm:
            case RHI_Format::RGBA8_SRGB:
            case RHI_Format::BGRA8_UNorm:
            case RHI_Format::RGBA16_Float:
            case RHI_Format::RGBA32_Float:
                return Shader::ShaderValueType::Float4;
            default:
                return Shader::ShaderValueType::Unknown;
            }
        }

        /**
         * @brief 把 Vulkan Validation Layer 消息转发到 ChikaEngine 日志系统。
         *
         * 统一日志出口让验证错误与引擎日志使用相同时间线，便于定位发生错误的具名 Pass 和资源。
         */
        VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*)
        {
            const char* message = callbackData && callbackData->pMessage ? callbackData->pMessage : "Unknown Vulkan validation message";
            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                LOG_ERROR("VulkanValidation", "{}", message);
            else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
                LOG_WARN("VulkanValidation", "{}", message);
            else
                LOG_INFO("VulkanValidation", "{}", message);
            return VK_FALSE;
        }

        /**
         * @brief 构造 Debug Messenger 配置，同时供实例创建阶段和运行阶段使用。
         *
         * 在 vkCreateInstance 的 pNext 中复用该配置，可以捕获实例创建期间产生的验证消息。
         */
        VkDebugUtilsMessengerCreateInfoEXT BuildDebugMessengerCreateInfo()
        {
            VkDebugUtilsMessengerCreateInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
            // Loader 的 Verbose/Info 输出量很大且通常不可操作，基线阶段只保留需要处理的 Warning/Error。
            info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            info.pfnUserCallback = VulkanDebugCallback;
            return info;
        }
    } // namespace

    VulkanRHIDevice::VulkanRHIDevice(const RHI_InitParams& params)
    {
        Initialize(params);
    }

    VulkanRHIDevice::~VulkanRHIDevice()
    {
        Shutdown();
    }

    void VulkanRHIDevice::Initialize(const RHI_InitParams& params)
    {
        // 先赋值参数 后期在 初始化 vulkan 的时候再做 check
        m_windowHandle = params.nativeWindowHandle;
        m_height = params.height;
        m_width = params.width;
        m_pipelineCachePath = params.pipelineCachePath;
        m_enableValidation = params.enableValidation;

        CreateInstance();
        CreateDebugMessenger();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreatePipelineCache();
        CreateAllocator();
        CreateDescriptorInfrastructure();
        CreateCommandPools();
        CreateSwapchain();
        CreateSyncObjects();
    }
    void VulkanRHIDevice::Shutdown()
    {
        if (!m_device)
            return;
        vkDeviceWaitIdle(m_device);
        SavePipelineCache();

        // for (auto imageView : m_swapchainImageViews)
        // {
        //     vkDestroyImageView(m_device, imageView, nullptr);
        // }
        // vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        CleanupSwapchain();

        for (uint32_t i = 0; i < m_imageCount; i++)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        }

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            // vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
            vkDestroyCommandPool(m_device, m_commandPools[i], nullptr);
            vkDestroyDescriptorPool(m_device, m_descriptorPools[i], nullptr);
            vkDestroyQueryPool(m_device, m_timestampQueryPools[i], nullptr);
        }

        if (m_persistentDescriptorPool)
            vkDestroyDescriptorPool(m_device, m_persistentDescriptorPool, nullptr);
        vkDestroySampler(m_device, m_defaultSampler, nullptr);

        m_buffers.ForEach([&](auto h, VulkanBuffer& vb) { vmaDestroyBuffer(m_allocator, vb.buffer, vb.allocation); });
        m_textureViews.ForEach(
            [&](auto, VulkanTextureView& view)
            {
                if (view.view)
                    vkDestroyImageView(m_device, view.view, nullptr);
            });
        m_textures.ForEach(
            [&](auto h, VulkanTexture& tex)
            {
                if (tex.view)
                    vkDestroyImageView(m_device, tex.view, nullptr);
                if (tex.allocation)
                    vmaDestroyImage(m_allocator, tex.image, tex.allocation);
            });
        m_shaders.ForEach(
            [&](auto h, VulkanShader& vs)
            {
                if (vs.module)
                    vkDestroyShaderModule(m_device, vs.module, nullptr);
            });

        m_pipelines.ForEach(
            [&](auto h, VulkanPipeline& pso)
            {
                if (pso.pipeline)
                    vkDestroyPipeline(m_device, pso.pipeline, nullptr);
            });
        m_samplers.ForEach(
            [&](auto, VulkanSampler& sampler)
            {
                if (sampler.sampler)
                    vkDestroySampler(m_device, sampler.sampler, nullptr);
            });
        for (const auto& [hash, layout] : m_pipelineLayoutCache)
            vkDestroyPipelineLayout(m_device, layout, nullptr);
        for (const auto& [hash, layout] : m_descriptorSetLayoutCache)
            vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
        if (m_pipelineCache)
            vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
        vmaDestroyAllocator(m_allocator);

        vkDestroyDevice(m_device, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        DestroyDebugMessenger();
        vkDestroyInstance(m_instance, nullptr);

        m_buffers.Clear();
        m_textures.Clear();
        m_shaders.Clear();
        m_pipelines.Clear();
        m_samplers.Clear();
        m_textureViews.Clear();
        m_pipelineLayoutCache.clear();
        m_descriptorSetLayoutCache.clear();
        m_persistentDescriptorSetCache.clear();

        m_allocator = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_surface = VK_NULL_HANDLE;
        m_instance = VK_NULL_HANDLE;
        m_physicalDevice = VK_NULL_HANDLE;
        m_pipelineCache = VK_NULL_HANDLE;
    }

    void VulkanRHIDevice::BeginFrame()
    {
        // 每帧从零开始计数，保证 Editor 和自动测试读取到的是单帧数据。
        m_frameStatistics.Reset();
        m_frameSkipped = false;
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

        m_passGpuTimings.clear();
        const auto& completedScopes = m_timestampScopes[m_currentFrame];
        if (!completedScopes.empty())
        {
            std::vector<uint64_t> timestamps(completedScopes.size() * 2u);
            const VkResult timestampResult = vkGetQueryPoolResults(m_device, m_timestampQueryPools[m_currentFrame], 0, static_cast<uint32_t>(timestamps.size()), timestamps.size() * sizeof(uint64_t), timestamps.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
            if (timestampResult == VK_SUCCESS)
            {
                for (size_t index = 0; index < completedScopes.size(); ++index)
                {
                    const uint64_t begin = timestamps[index * 2u];
                    const uint64_t end = timestamps[index * 2u + 1u];
                    m_passGpuTimings.push_back({ completedScopes[index].name, static_cast<double>(end - begin) * m_timestampPeriodNs / 1'000'000.0 });
                }
            }
        }
        m_timestampScopes[m_currentFrame].clear();
        m_timestampQueriesReset = false;

        VkResult res = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_currentImageIndex);

        if (res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_frameSkipped = true; // 告诉底层本帧作废
            return;
        }
        else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
        {
            LOG_ERROR("Vulkan", "Failed to acquire swapchain image");
            return;
        }

        vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
        vkResetCommandPool(m_device, m_commandPools[m_currentFrame], 0);
        vkResetDescriptorPool(m_device, m_descriptorPools[m_currentFrame], 0);

        m_pendingCmds[m_currentFrame].clear();
        m_submittedCommandLists[m_currentFrame].clear();
        FlushDeletionQueue();
    }

    void VulkanRHIDevice::EndFrame()
    {
        // 获得图像失败, 先跳过
        if (m_frameSkipped)
            return;

        // Submit
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
        submitInfo.pWaitDstStageMask = waitStages;

        if (!m_pendingCmds[m_currentFrame].empty())
        {
            submitInfo.commandBufferCount = static_cast<uint32_t>(m_pendingCmds[m_currentFrame].size());
            submitInfo.pCommandBuffers = m_pendingCmds[m_currentFrame].data();
        }
        else
        {
            submitInfo.commandBufferCount = 0;
            submitInfo.pCommandBuffers = nullptr;
        }

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentImageIndex];

        VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]), "Failed to submit graphics queue");

        // Present
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        // Queue Submit 始终等待 imageAvailable 并 signal renderFinished，即使本帧没有命令。
        // Present 必须消费该 signal，否则下一次复用 binary semaphore 会触发重复 signal。
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentImageIndex];
        // presentInfo.waitSemaphoreCount = 1;
        // presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_swapchain;
        presentInfo.pImageIndices = &m_currentImageIndex;

        // 如果呈现的时候出现问题——
        VkResult res = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
        if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR && res != VK_SUBOPTIMAL_KHR)
        {
            LOG_ERROR("Vulkan", "Failed to present");
        }

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        m_absoluteFrame++;
    }

    void VulkanRHIDevice::Submit(IRHICommandList* cmdList)
    {
        VulkanCommandList* vCmd = static_cast<VulkanCommandList*>(cmdList);
        m_pendingCmds[m_currentFrame].push_back(vCmd->GetVkCmdRaw());
        m_submittedCommandLists[m_currentFrame].emplace_back(cmdList);
    }

    BufferHandle VulkanRHIDevice::CreateBuffer(const BufferDesc& desc)
    {
        if (desc.size == 0)
        {
            LOG_ERROR("Vulkan", "Attempted to create a buffer with size 0! Usage Enum: {}", (int)desc.usage);
            return BufferHandle{};
        }

        VkBufferCreateInfo bInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bInfo.size = desc.size;
        bInfo.usage = 0;

        if ((desc.usage & RHI_BufferUsage::Vertex) != RHI_BufferUsage::None)
            bInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if ((desc.usage & RHI_BufferUsage::Index) != RHI_BufferUsage::None)
            bInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if ((desc.usage & RHI_BufferUsage::Uniform) != RHI_BufferUsage::None)
            bInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if ((desc.usage & RHI_BufferUsage::Storage) != RHI_BufferUsage::None)
            bInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if ((desc.usage & RHI_BufferUsage::TransferSrc) != RHI_BufferUsage::None)
            bInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if ((desc.usage & RHI_BufferUsage::TransferDst) != RHI_BufferUsage::None)
            bInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if ((desc.usage & RHI_BufferUsage::Indirect) != RHI_BufferUsage::None)
            bInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

        // GPU-only resources commonly receive initial data through a Copy Pass.
        if (desc.memoryUsage == MemoryUsage::GPU_Only)
            bInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (bInfo.usage == 0)
        {
            LOG_ERROR("Vulkan", "Buffer usage is 0! VMA allocation will fail.");
            return BufferHandle{};
        }

        VmaAllocationCreateInfo aInfo{};
        if (desc.memoryUsage == MemoryUsage::GPU_Only)
        {
            aInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        }
        else
        {
            aInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            aInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VulkanBuffer vb;
        VK_CHECK(vmaCreateBuffer(m_allocator, &bInfo, &aInfo, &vb.buffer, &vb.allocation, &vb.allocInfo), "VMA buffer alloc failed");
        return m_buffers.Create(vb);
    }

    TextureHandle VulkanRHIDevice::CreateTexture(const TextureDesc& desc)
    {
        VkFormat vkFmt = ToVkFormat(desc.format);
        VkImageCreateInfo iInfo{};
        iInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        iInfo.imageType = VK_IMAGE_TYPE_2D;
        iInfo.extent = { desc.width, desc.height, 1 };
        iInfo.mipLevels = desc.mipLevels;
        iInfo.arrayLayers = desc.arrayLayers;
        iInfo.format = vkFmt;
        iInfo.samples = ToVkSampleCount(desc.sampleCount);

        iInfo.usage = ToVkUsage(desc.usage, desc.format);

        VmaAllocationCreateInfo aInfo{};
        aInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VulkanTexture vt;
        vt.format = vkFmt;
        vt.width = desc.width;
        vt.height = desc.height;
        vt.mipLevels = desc.mipLevels;
        vt.arrayLayers = desc.arrayLayers;
        VK_CHECK(vmaCreateImage(m_allocator, &iInfo, &aInfo, &vt.image, &vt.allocation, nullptr), "VMA image alloc failed");

        VkImageViewCreateInfo vInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vInfo.image = vt.image;
        vInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vInfo.format = vkFmt;
        vInfo.subresourceRange.aspectMask = (desc.format == RHI_Format::D32_SFloat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        vInfo.subresourceRange.levelCount = desc.mipLevels;
        vInfo.subresourceRange.layerCount = desc.arrayLayers;
        VK_CHECK(vkCreateImageView(m_device, &vInfo, nullptr, &vt.view), "Failed to create Image View");

        return m_textures.Create(vt);
    }

    /**
     * @brief 创建可独立绑定到 Reflection Sampler Descriptor 的 Vulkan Sampler。
     */
    SamplerHandle VulkanRHIDevice::CreateSampler(const SamplerDesc& desc)
    {
        VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        info.minFilter = ToVkFilter(desc.minFilter);
        info.magFilter = ToVkFilter(desc.magFilter);
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = ToVkAddressMode(desc.addressU);
        info.addressModeV = ToVkAddressMode(desc.addressV);
        info.addressModeW = ToVkAddressMode(desc.addressW);
        info.anisotropyEnable = desc.anisotropyEnable ? VK_TRUE : VK_FALSE;
        info.maxAnisotropy = desc.maxAnisotropy;
        info.maxLod = VK_LOD_CLAMP_NONE;
        VulkanSampler sampler;
        VK_CHECK(vkCreateSampler(m_device, &info, nullptr, &sampler.sampler), "Failed to create Sampler");
        return m_samplers.Create(sampler);
    }

    /**
     * @brief 创建可绑定指定 mip/layer 范围的 Vulkan Image View。
     */
    TextureViewHandle VulkanRHIDevice::CreateTextureView(const TextureViewDesc& desc)
    {
        VulkanTexture* texture = m_textures.Get(desc.texture);
        if (!texture)
            return TextureViewHandle::Invalid();
        const uint32_t mipCount = desc.range.mipLevelCount == 0 ? texture->mipLevels - desc.range.baseMipLevel : desc.range.mipLevelCount;
        const uint32_t layerCount = desc.range.arrayLayerCount == 0 ? texture->arrayLayers - desc.range.baseArrayLayer : desc.range.arrayLayerCount;

        VkImageViewCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        info.image = texture->image;
        info.viewType = layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        info.format = texture->format;
        const bool isDepth = texture->format == VK_FORMAT_D32_SFLOAT || texture->format == VK_FORMAT_D24_UNORM_S8_UINT;
        info.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        if (texture->format == VK_FORMAT_D24_UNORM_S8_UINT)
            info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        info.subresourceRange.baseMipLevel = desc.range.baseMipLevel;
        info.subresourceRange.levelCount = mipCount;
        info.subresourceRange.baseArrayLayer = desc.range.baseArrayLayer;
        info.subresourceRange.layerCount = layerCount;

        VulkanTextureView view{ .texture = desc.texture };
        VK_CHECK(vkCreateImageView(m_device, &info, nullptr, &view.view), "Failed to create Texture View");
        return m_textureViews.Create(view);
    }

    ShaderHandle VulkanRHIDevice::CreateShader(const ShaderDesc& desc)
    {
        VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        info.codeSize = desc.codeSize;
        info.pCode = reinterpret_cast<const uint32_t*>(desc.code);
        VulkanShader vs;
        VK_CHECK(vkCreateShaderModule(m_device, &info, nullptr, &vs.module), "Failed to compile shader module");
        return m_shaders.Create(vs);
    }

    PipelineHandle VulkanRHIDevice::CreateGraphicsPipeline(const PipelineDesc& desc)
    {
        VulkanShader* vs = m_shaders.Get(desc.vertexShader);
        VulkanShader* fs = m_shaders.Get(desc.fragmentShader);
        if (!vs)
            throw std::runtime_error("Graphics Pipeline references an invalid Vertex Shader Handle");
        if (desc.fragmentShader.IsValid() && !fs)
            throw std::runtime_error("Graphics Pipeline references an invalid Fragment Shader Handle");
        if (!desc.shaderInterface.fragmentOutputs.empty() && desc.shaderInterface.fragmentOutputs.size() != desc.colorAttachmentFormats.size())
            throw std::runtime_error("Fragment output count does not match graphics pipeline color attachments");
        for (const auto& output : desc.shaderInterface.fragmentOutputs)
        {
            if (output.location >= desc.colorAttachmentFormats.size() || output.type != AttachmentValueType(desc.colorAttachmentFormats[output.location]))
                throw std::runtime_error("Fragment output type does not match graphics pipeline color attachment");
        }

        // Pipeline Layout 完全由合并后的 Shader Interface 生成，避免后端继续保留固定布局假设。
        VulkanPipeline pso = CreatePipelineLayout(desc.shaderInterface);

        VkPipelineShaderStageCreateInfo shaderStages[2] = {};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vs->module;
        shaderStages[0].pName = "main";

        if (fs)
        {
            shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderStages[1].module = fs->module;
            shaderStages[1].pName = "main";
        }

        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.stride = desc.vertexLayout.stride;
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attrDescs;
        for (const auto& attr : desc.vertexLayout.attributes)
        {
            attrDescs.push_back({ attr.location, 0, ToVkFormat(attr.format), attr.offset });
        }

        const bool hasVertexInput = desc.vertexLayout.stride > 0 && !attrDescs.empty();
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = hasVertexInput ? 1u : 0u;
        vertexInputInfo.pVertexBindingDescriptions = hasVertexInput ? &bindingDesc : nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = attrDescs.size();
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs.empty() ? nullptr : attrDescs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = ToVkTopology(desc.topology);

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = ToVkCullMode(desc.cullMode);
        rasterizer.frontFace = ToVkFrontFace(desc.frontFace);
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = desc.depthTest ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = ToVkCompareOp(desc.depthCompare);

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(desc.colorAttachmentFormats.size());
        for (auto& cba : colorBlendAttachments)
        {
            cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            cba.blendEnable = desc.alphaBlendEnable ? VK_TRUE : VK_FALSE;
            cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            cba.colorBlendOp = VK_BLEND_OP_ADD;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            cba.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = colorBlendAttachments.size();
        colorBlending.pAttachments = colorBlendAttachments.empty() ? nullptr : colorBlendAttachments.data();

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicStateInfo.dynamicStateCount = dynamicStates.size();
        dynamicStateInfo.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        std::vector<VkFormat> vkColorFormats;
        for (auto f : desc.colorAttachmentFormats)
            vkColorFormats.push_back(ToVkFormat(f));

        VkPipelineRenderingCreateInfo renderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        renderingInfo.colorAttachmentCount = vkColorFormats.size();
        renderingInfo.pColorAttachmentFormats = vkColorFormats.empty() ? nullptr : vkColorFormats.data();
        renderingInfo.depthAttachmentFormat = desc.depthAttachmentFormat == RHI_Format::Unknown ? VK_FORMAT_UNDEFINED : ToVkFormat(desc.depthAttachmentFormat);

        VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.pNext = &renderingInfo;
        pipelineInfo.stageCount = fs ? 2u : 1u;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicStateInfo;
        pipelineInfo.layout = pso.layout;
        pipelineInfo.renderPass = VK_NULL_HANDLE;

        VK_CHECK(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineInfo, nullptr, &pso.pipeline), "Failed to create Graphics Pipeline");

        return m_pipelines.Create(pso);
    }

    /**
     * @brief 按 Compute Shader Reflection 创建 Pipeline Layout 与 Vulkan Compute Pipeline。
     */
    PipelineHandle VulkanRHIDevice::CreateComputePipeline(const ComputePipelineDesc& desc)
    {
        VulkanShader* computeShader = m_shaders.Get(desc.computeShader);
        if (!computeShader)
            throw std::runtime_error("Compute Pipeline references an invalid Shader Handle");

        VulkanPipeline pipeline = CreatePipelineLayout(desc.shaderInterface);
        pipeline.bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

        VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = computeShader->module;
        stage.pName = "main";

        VkComputePipelineCreateInfo createInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        createInfo.stage = stage;
        createInfo.layout = pipeline.layout;
        VK_CHECK(vkCreateComputePipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &pipeline.pipeline), "Failed to create Compute Pipeline");
        return m_pipelines.Create(pipeline);
    }

    void* VulkanRHIDevice::GetMappedData(BufferHandle handle)
    {
        return m_buffers.Get(handle)->allocInfo.pMappedData;
    }

    /**
     * @brief 为 Buffer 设置 Vulkan Debug Utils 名称。
     *
     * Handle 无效或 Debug Utils 未启用时静默跳过，避免诊断功能改变资源生命周期行为。
     */
    void VulkanRHIDevice::SetDebugName(BufferHandle handle, std::string_view name)
    {
        const VulkanBuffer* buffer = m_buffers.Get(handle);
        if (buffer)
            SetVulkanObjectName(VK_OBJECT_TYPE_BUFFER, ToDebugObjectHandle(buffer->buffer), name);
    }

    /**
     * @brief 为 Texture 的 Image 和 ImageView 设置关联名称。
     *
     * 同时命名 ImageView，便于验证层报告具体引用视图时仍能关联回上层纹理。
     */
    void VulkanRHIDevice::SetDebugName(TextureHandle handle, std::string_view name)
    {
        const VulkanTexture* texture = m_textures.Get(handle);
        if (!texture)
            return;

        SetVulkanObjectName(VK_OBJECT_TYPE_IMAGE, ToDebugObjectHandle(texture->image), name);
        const std::string viewName = std::string(name) + ".View";
        SetVulkanObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, ToDebugObjectHandle(texture->view), viewName);
    }

    void VulkanRHIDevice::SetDebugName(SamplerHandle handle, std::string_view name)
    {
        const VulkanSampler* sampler = m_samplers.Get(handle);
        if (sampler)
            SetVulkanObjectName(VK_OBJECT_TYPE_SAMPLER, ToDebugObjectHandle(sampler->sampler), name);
    }

    void VulkanRHIDevice::SetDebugName(TextureViewHandle handle, std::string_view name)
    {
        const VulkanTextureView* view = m_textureViews.Get(handle);
        if (view)
            SetVulkanObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, ToDebugObjectHandle(view->view), name);
    }

    /**
     * @brief 为 Shader Module 设置调试名称，帮助 Pipeline 创建错误指向具体 Shader。
     */
    void VulkanRHIDevice::SetDebugName(ShaderHandle handle, std::string_view name)
    {
        const VulkanShader* shader = m_shaders.Get(handle);
        if (shader)
            SetVulkanObjectName(VK_OBJECT_TYPE_SHADER_MODULE, ToDebugObjectHandle(shader->module), name);
    }

    /**
     * @brief 为 Graphics Pipeline 及其 Pipeline Layout 设置关联名称。
     *
     * Pipeline 和 Layout 共同命名，能够让 Descriptor 或 Push Constant 验证错误指向同一业务 Pipeline。
     */
    void VulkanRHIDevice::SetDebugName(PipelineHandle handle, std::string_view name)
    {
        const VulkanPipeline* pipeline = m_pipelines.Get(handle);
        if (!pipeline)
            return;

        SetVulkanObjectName(VK_OBJECT_TYPE_PIPELINE, ToDebugObjectHandle(pipeline->pipeline), name);
        const std::string layoutName = std::string(name) + ".Layout";
        SetVulkanObjectName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, ToDebugObjectHandle(pipeline->layout), layoutName);
    }

    /**
     * @brief 为 Vulkan 对象提交 Debug Utils 名称。
     *
     * 函数指针按需加载，使未启用 Validation/Debug Utils 的路径保持可运行。
     */
    void VulkanRHIDevice::SetVulkanObjectName(VkObjectType type, uint64_t objectHandle, std::string_view name)
    {
        if (!m_enableValidation || objectHandle == 0 || name.empty())
            return;

        const auto setObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(m_device, "vkSetDebugUtilsObjectNameEXT"));
        if (!setObjectName)
            return;

        const std::string stableName(name);
        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.objectType = type;
        info.objectHandle = objectHandle;
        info.pObjectName = stableName.c_str();
        setObjectName(m_device, &info);
    }

    /**
     * @brief 累加一次 Draw 及其实例数量。
     *
     * Draw Call 和 Instance 分开统计，为后续 Instancing 优化提供明确基线。
     */
    void VulkanRHIDevice::RecordDraw(uint32_t instanceCount)
    {
        ++m_frameStatistics.drawCallCount;
        m_frameStatistics.instanceCount += instanceCount;
    }

    /**
     * @brief 累加一次 Pipeline Bind。
     */
    void VulkanRHIDevice::RecordPipelineBind()
    {
        ++m_frameStatistics.pipelineBindCount;
    }

    /**
     * @brief 累加本次 vkUpdateDescriptorSets 实际写入的 Descriptor 数量。
     */
    void VulkanRHIDevice::RecordDescriptorUpdates(uint32_t descriptorCount)
    {
        m_frameStatistics.descriptorUpdateCount += descriptorCount;
    }

    /**
     * @brief 每帧仅在第一个 Command Buffer 中重置 Query Pool。
     */
    void VulkanRHIDevice::PrepareTimestampQueries(VkCommandBuffer commandBuffer)
    {
        if (m_timestampQueriesReset)
            return;
        vkCmdResetQueryPool(commandBuffer, m_timestampQueryPools[m_currentFrame], 0, MAX_TIMESTAMP_QUERIES);
        m_timestampQueriesReset = true;
    }

    /**
     * @brief 为一个 Pass 分配连续 begin/end Timestamp Query。
     */
    uint32_t VulkanRHIDevice::AllocateTimestampScope(std::string_view name)
    {
        const uint32_t beginQuery = static_cast<uint32_t>(m_timestampScopes[m_currentFrame].size() * 2u);
        if (beginQuery + 1u >= MAX_TIMESTAMP_QUERIES)
            return UINT32_MAX;
        m_timestampScopes[m_currentFrame].push_back({ std::string(name), beginQuery, beginQuery + 1u });
        return beginQuery;
    }

    IRHICommandList* VulkanRHIDevice::AllocateCommandList()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPools[m_currentFrame];
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, &cmd), "Failed to allocate command buffer");
        return new VulkanCommandList(this, cmd);
    }

    VulkanRHIDevice::DescriptorAllocation VulkanRHIDevice::AllocateDescriptorSet(const VulkanPipeline& pipeline, const ResourceBindingGroup& group)
    {
        if (group.set >= pipeline.setLayouts.size())
            throw std::runtime_error("Resource binding group targets a set not declared by the current pipeline");

        const VkDescriptorSetLayout layout = pipeline.setLayouts[group.set];
        if (group.lifetime == ResourceBindingLifetime::Persistent)
        {
            const uint64_t cacheKey = HashResourceBindingGroup(group, layout);
            const auto existing = m_persistentDescriptorSetCache.find(cacheKey);
            if (existing != m_persistentDescriptorSetCache.end())
                return { existing->second, false };

            VkDescriptorSetAllocateInfo persistentInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            persistentInfo.descriptorPool = m_persistentDescriptorPool;
            persistentInfo.descriptorSetCount = 1;
            persistentInfo.pSetLayouts = &layout;
            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
            VK_CHECK(vkAllocateDescriptorSets(m_device, &persistentInfo, &descriptorSet), "Failed to allocate persistent descriptor set");
            m_persistentDescriptorSetCache.emplace(cacheKey, descriptorSet);
            return { descriptorSet, true };
        }

        VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = m_descriptorPools[m_currentFrame];
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet), "Failed to allocate transient descriptor set");
        return { descriptorSet, true };
    }

    void VulkanRHIDevice::CreateInstance()
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        createInfo.pApplicationInfo = &appInfo;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (m_enableValidation)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (m_enableValidation)
        {
            // 在实例创建阶段提前挂接回调，确保实例初始化问题也进入统一日志。
            debugCreateInfo = BuildDebugMessengerCreateInfo();
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            createInfo.pNext = &debugCreateInfo;
        }

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance), "Failed to create Vulkan instance");
    }

    /**
     * @brief 创建运行阶段 Vulkan Debug Messenger。
     *
     * 使用实例函数指针加载扩展入口，避免对 Vulkan Loader 导出符号作额外假设。
     */
    void VulkanRHIDevice::CreateDebugMessenger()
    {
        if (!m_enableValidation || !m_instance)
            return;

        const auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (!createMessenger)
            return;

        const VkDebugUtilsMessengerCreateInfoEXT info = BuildDebugMessengerCreateInfo();
        VK_CHECK(createMessenger(m_instance, &info, nullptr, &m_debugMessenger), "Failed to create Vulkan debug messenger");
    }

    /**
     * @brief 销毁 Vulkan Debug Messenger。
     *
     * Messenger 必须在 Instance 前销毁，保持 Vulkan 对象生命周期顺序正确。
     */
    void VulkanRHIDevice::DestroyDebugMessenger()
    {
        if (!m_debugMessenger || !m_instance)
            return;

        const auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyMessenger)
            destroyMessenger(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    void VulkanRHIDevice::CreateSurface()
    {
        if (!m_windowHandle)
            LOG_ERROR("Vulkan", "No window input");
        auto glfwWindowHandle = static_cast<GLFWwindow*>(m_windowHandle);
        VK_CHECK(glfwCreateWindowSurface(m_instance, glfwWindowHandle, nullptr, &m_surface), "Failed to create window surface");
    }

    void VulkanRHIDevice::PickPhysicalDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        if (count == 0)
            throw std::runtime_error("No Vulkan physical device found");

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        // TODO: 实现评估方法
        m_physicalDevice = devices[0];
    }

    void VulkanRHIDevice::CreateLogicalDevice()
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, families.data());

        m_graphicsQueueFamily = 0;
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                m_graphicsQueueFamily = i;
                break;
            }
        }

        float priority = 1.0f;
        VkDeviceQueueCreateInfo qinfo{};
        qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qinfo.queueFamilyIndex = m_graphicsQueueFamily;
        qinfo.queueCount = 1;
        qinfo.pQueuePriorities = &priority;

        VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        features13.dynamicRendering = VK_TRUE;

        VkDeviceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &qinfo;
        info.pNext = &features13;

        const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = deviceExtensions;

        VK_CHECK(vkCreateDevice(m_physicalDevice, &info, nullptr, &m_device), "vkCreateDevice failed");
        vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    }

    void VulkanRHIDevice::CreateAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = m_physicalDevice;
        allocatorInfo.device = m_device;
        allocatorInfo.instance = m_instance;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
        allocatorInfo.pVulkanFunctions = &vulkanFunctions;

        VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_allocator), "Failed to create VMA allocator");
    }

    /**
     * @brief 创建 Reflection Descriptor 系统使用的默认 Sampler 和 Descriptor Pool。
     *
     * Pool 覆盖 RHI 可表达的 Descriptor 类型，具体 Set Layout 在创建 Pipeline 时按需缓存。
     */
    void VulkanRHIDevice::CreateDescriptorInfrastructure()
    {
        VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_defaultSampler), "Failed to create default sampler");
        SetVulkanObjectName(VK_OBJECT_TYPE_SAMPLER, ToDebugObjectHandle(m_defaultSampler), "RHI.DefaultSampler");

        const std::array<VkDescriptorPoolSize, 6> poolSizes = {
            VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 512 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2048 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 512 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 512 }, VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, 256 },
        };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            poolInfo.maxSets = 1024;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            VK_CHECK(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPools[i]), "Failed to create descriptor pool");
        }

        VkDescriptorPoolCreateInfo persistentPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        persistentPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        persistentPoolInfo.maxSets = 4096;
        persistentPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        persistentPoolInfo.pPoolSizes = poolSizes.data();
        VK_CHECK(vkCreateDescriptorPool(m_device, &persistentPoolInfo, nullptr, &m_persistentDescriptorPool), "Failed to create persistent descriptor pool");
    }

    /**
     * @brief 创建或复用一个完全由 Reflection Binding 描述的 VkDescriptorSetLayout。
     */
    VkDescriptorSetLayout VulkanRHIDevice::GetOrCreateDescriptorSetLayout(std::span<const Shader::ShaderResourceBinding> bindings)
    {
        const uint64_t hash = HashDescriptorSetLayout(bindings);
        const auto cached = m_descriptorSetLayoutCache.find(hash);
        if (cached != m_descriptorSetLayoutCache.end())
            return cached->second;

        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        vkBindings.reserve(bindings.size());
        for (const auto& binding : bindings)
        {
            vkBindings.push_back({
                .binding = binding.binding,
                .descriptorType = ToVkDescriptorType(binding.type),
                .descriptorCount = binding.arrayCount,
                .stageFlags = ToVkShaderStages(binding.stages),
            });
        }

        VkDescriptorSetLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        createInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
        createInfo.pBindings = vkBindings.empty() ? nullptr : vkBindings.data();
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &layout), "Failed to create reflected descriptor set layout");
        m_descriptorSetLayoutCache.emplace(hash, layout);
        return layout;
    }

    /**
     * @brief 根据合并 Shader Interface 创建并缓存 Pipeline Layout。
     *
     * Set Layout 与 Pipeline Layout 均以稳定 Interface Hash 复用，Pipeline 仅持有 Vulkan Handle 引用。
     */
    VulkanPipeline VulkanRHIDevice::CreatePipelineLayout(const Shader::ShaderProgramInterface& interface)
    {
        VulkanPipeline pipeline{};
        Shader::ShaderReflectionData layoutReflection{
            .resources = interface.resources,
            .pushConstants = interface.pushConstants,
        };
        pipeline.interfaceHash = Shader::HashShaderReflection(layoutReflection);
        pipeline.resources = interface.resources;
        pipeline.pushConstants = interface.pushConstants;

        uint32_t maxSet = 0;
        for (const auto& resource : interface.resources)
            maxSet = std::max(maxSet, resource.set);
        if (!interface.resources.empty())
            pipeline.setLayouts.resize(maxSet + 1);

        for (uint32_t set = 0; set < pipeline.setLayouts.size(); ++set)
        {
            std::vector<Shader::ShaderResourceBinding> setBindings;
            for (const auto& resource : interface.resources)
            {
                if (resource.set == set)
                    setBindings.push_back(resource);
            }
            pipeline.setLayouts[set] = GetOrCreateDescriptorSetLayout(setBindings);
        }

        const auto cached = m_pipelineLayoutCache.find(pipeline.interfaceHash);
        if (cached != m_pipelineLayoutCache.end())
        {
            pipeline.layout = cached->second;
            return pipeline;
        }

        std::vector<VkPushConstantRange> ranges;
        ranges.reserve(interface.pushConstants.size());
        for (const auto& range : interface.pushConstants)
            ranges.push_back({ ToVkShaderStages(range.stages), range.offset, range.size });

        VkPipelineLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        createInfo.setLayoutCount = static_cast<uint32_t>(pipeline.setLayouts.size());
        createInfo.pSetLayouts = pipeline.setLayouts.empty() ? nullptr : pipeline.setLayouts.data();
        createInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
        createInfo.pPushConstantRanges = ranges.empty() ? nullptr : ranges.data();
        VK_CHECK(vkCreatePipelineLayout(m_device, &createInfo, nullptr, &pipeline.layout), "Failed to create reflected pipeline layout");
        m_pipelineLayoutCache.emplace(pipeline.interfaceHash, pipeline.layout);
        return pipeline;
    }
    void VulkanRHIDevice::CreateCommandPools()
    {
        VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.queueFamilyIndex = m_graphicsQueueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPools[i]), "Failed to create command pool");
        }
    }

    void VulkanRHIDevice::CreateSwapchain()
    {
        // auto glfwWindowHandle = static_cast<GLFWwindow*>(m_windowHandle);
        m_swapchainExtent = { m_width, m_height };

        // TODO[x]: 检查是否支持

        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            m_swapchainExtent = capabilities.currentExtent;
        }
        else
        {
            // 严格钳制在允许的大小范围内
            m_swapchainExtent.width = std::clamp(m_width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            m_swapchainExtent.height = std::clamp(m_height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        {
            imageCount = capabilities.maxImageCount;
        }

        m_swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = MAX_FRAMES_IN_FLIGHT + 1;
        createInfo.imageFormat = m_swapchainFormat;
        createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        createInfo.imageExtent = m_swapchainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain), "Failed to create swapchain");

        // uint32_t imageCount;
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, nullptr);

        m_swapchainImages.resize(m_imageCount);
        m_swapchainTextures.resize(m_imageCount);
        m_swapchainImageViews.resize(m_imageCount);

        vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, m_swapchainImages.data());

        for (size_t i = 0; i < m_imageCount; i++)
        {
            VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            viewInfo.image = m_swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_swapchainFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]);

            // 打包成 vulkan texture 前端可以调用
            VulkanTexture vt;
            vt.image = m_swapchainImages[i];
            vt.view = m_swapchainImageViews[i];
            vt.format = m_swapchainFormat;
            vt.width = m_swapchainExtent.width;
            vt.height = m_swapchainExtent.height;
            vt.allocation = nullptr;
            m_swapchainTextures[i] = m_textures.Create(vt);
            SetDebugName(m_swapchainTextures[i], "Swapchain.Image." + std::to_string(i));
        }
    }

    void VulkanRHIDevice::CreateSyncObjects()
    {
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        // m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]);
            // vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]);
            VkQueryPoolCreateInfo queryInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
            queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryInfo.queryCount = MAX_TIMESTAMP_QUERIES;
            VK_CHECK(vkCreateQueryPool(m_device, &queryInfo, nullptr, &m_timestampQueryPools[i]), "Failed to create timestamp query pool");
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
        m_timestampPeriodNs = properties.limits.timestampPeriod;

        m_renderFinishedSemaphores.resize(m_imageCount);
        for (uint32_t i = 0; i < m_imageCount; ++i)
        {
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
        }
    }

    void VulkanRHIDevice::DestroyBuffer(BufferHandle handle)
    {
        std::lock_guard<std::mutex> lock(m_deletionMutex);
        m_bufferDeletionQueue.push_back({ m_absoluteFrame, handle.raw_value });
    }

    void VulkanRHIDevice::DestroyTexture(TextureHandle handle)
    {
        std::lock_guard<std::mutex> lock(m_deletionMutex);
        m_textureDeletionQueue.push_back({ m_absoluteFrame, handle.raw_value });
    }

    void VulkanRHIDevice::DestroyShader(ShaderHandle handle)
    {
        std::lock_guard<std::mutex> lock(m_deletionMutex);
        m_shaderDeletionQueue.push_back({ m_absoluteFrame, handle.raw_value });
    }

    void VulkanRHIDevice::DestroyPipeline(PipelineHandle handle)
    {
        std::lock_guard<std::mutex> lock(m_deletionMutex);
        m_pipelineDeletionQueue.push_back({ m_absoluteFrame, handle.raw_value });
    }

    void VulkanRHIDevice::DestroySampler(SamplerHandle handle)
    {
        std::lock_guard<std::mutex> lock(m_deletionMutex);
        m_samplerDeletionQueue.push_back({ m_absoluteFrame, handle.raw_value });
    }

    void VulkanRHIDevice::DestroyTextureView(TextureViewHandle handle)
    {
        std::lock_guard<std::mutex> lock(m_deletionMutex);
        m_textureViewDeletionQueue.push_back({ m_absoluteFrame, handle.raw_value });
    }

    void VulkanRHIDevice::FlushDeletionQueue()
    {
        {
            auto it = m_bufferDeletionQueue.begin();
            while (it != m_bufferDeletionQueue.end())
            {
                if (m_absoluteFrame > it->frameIndex + MAX_FRAMES_IN_FLIGHT)
                {
                    BufferHandle h{ it->handleRaw };
                    VulkanBuffer* vb = m_buffers.Get(h);
                    if (vb)
                    {
                        vmaDestroyBuffer(m_allocator, vb->buffer, vb->allocation);
                        m_buffers.Destroy(h);
                    }
                    it = m_bufferDeletionQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        {
            auto it = m_textureDeletionQueue.begin();
            while (it != m_textureDeletionQueue.end())
            {
                if (m_absoluteFrame > it->frameIndex + MAX_FRAMES_IN_FLIGHT)
                {
                    TextureHandle h{ it->handleRaw };
                    VulkanTexture* tex = m_textures.Get(h);
                    if (tex)
                    {
                        if (tex->view)
                            vkDestroyImageView(m_device, tex->view, nullptr);
                        if (tex->allocation)
                            vmaDestroyImage(m_allocator, tex->image, tex->allocation);

                        tex->view = VK_NULL_HANDLE;
                        tex->image = VK_NULL_HANDLE;
                        tex->allocation = nullptr;

                        m_textures.Destroy(h);
                    }
                    it = m_textureDeletionQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        {
            auto it = m_shaderDeletionQueue.begin();
            while (it != m_shaderDeletionQueue.end())
            {
                if (m_absoluteFrame > it->frameIndex + MAX_FRAMES_IN_FLIGHT)
                {
                    ShaderHandle handle{ it->handleRaw };
                    VulkanShader* shader = m_shaders.Get(handle);
                    if (shader && shader->module)
                        vkDestroyShaderModule(m_device, shader->module, nullptr);
                    m_shaders.Destroy(handle);
                    it = m_shaderDeletionQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        {
            auto it = m_pipelineDeletionQueue.begin();
            while (it != m_pipelineDeletionQueue.end())
            {
                if (m_absoluteFrame > it->frameIndex + MAX_FRAMES_IN_FLIGHT)
                {
                    PipelineHandle handle{ it->handleRaw };
                    VulkanPipeline* pipeline = m_pipelines.Get(handle);
                    if (pipeline)
                    {
                        if (pipeline->pipeline)
                            vkDestroyPipeline(m_device, pipeline->pipeline, nullptr);
                    }
                    m_pipelines.Destroy(handle);
                    it = m_pipelineDeletionQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        {
            auto it = m_samplerDeletionQueue.begin();
            while (it != m_samplerDeletionQueue.end())
            {
                if (m_absoluteFrame > it->frameIndex + MAX_FRAMES_IN_FLIGHT)
                {
                    SamplerHandle handle{ it->handleRaw };
                    VulkanSampler* sampler = m_samplers.Get(handle);
                    if (sampler && sampler->sampler)
                        vkDestroySampler(m_device, sampler->sampler, nullptr);
                    m_samplers.Destroy(handle);
                    it = m_samplerDeletionQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        {
            auto it = m_textureViewDeletionQueue.begin();
            while (it != m_textureViewDeletionQueue.end())
            {
                if (m_absoluteFrame > it->frameIndex + MAX_FRAMES_IN_FLIGHT)
                {
                    TextureViewHandle handle{ it->handleRaw };
                    VulkanTextureView* view = m_textureViews.Get(handle);
                    if (view && view->view)
                        vkDestroyImageView(m_device, view->view, nullptr);
                    m_textureViews.Destroy(handle);
                    it = m_textureViewDeletionQueue.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    TextureHandle VulkanRHIDevice::GetActiveSwapchainTexture()
    {
        return m_swapchainTextures[m_currentImageIndex];
    }

    void VulkanRHIDevice::WaitIdle()
    {
        if (m_device)
            vkDeviceWaitIdle(m_device);
    }

    void VulkanRHIDevice::CreatePipelineCache()
    {
        std::vector<uint8_t> initialData;
        if (!m_pipelineCachePath.empty())
        {
            std::ifstream file(m_pipelineCachePath, std::ios::binary);
            if (file)
                initialData.assign(std::istreambuf_iterator<char>(file), {});
        }

        VkPipelineCacheCreateInfo createInfo{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
        createInfo.initialDataSize = initialData.size();
        createInfo.pInitialData = initialData.empty() ? nullptr : initialData.data();
        VkResult result = vkCreatePipelineCache(m_device, &createInfo, nullptr, &m_pipelineCache);
        if (result != VK_SUCCESS && !initialData.empty())
        {
            LOG_WARN("Vulkan", "Ignoring incompatible pipeline cache {}", m_pipelineCachePath.string());
            createInfo.initialDataSize = 0;
            createInfo.pInitialData = nullptr;
            VK_CHECK(vkCreatePipelineCache(m_device, &createInfo, nullptr, &m_pipelineCache), "Failed to create empty pipeline cache");
        }
        else
        {
            VK_CHECK(result, "Failed to create pipeline cache");
        }
    }

    void VulkanRHIDevice::SavePipelineCache()
    {
        if (!m_pipelineCache || m_pipelineCachePath.empty())
            return;

        size_t dataSize = 0;
        if (vkGetPipelineCacheData(m_device, m_pipelineCache, &dataSize, nullptr) != VK_SUCCESS || dataSize == 0)
            return;

        std::vector<uint8_t> data(dataSize);
        if (vkGetPipelineCacheData(m_device, m_pipelineCache, &dataSize, data.data()) != VK_SUCCESS)
            return;

        std::error_code error;
        std::filesystem::create_directories(m_pipelineCachePath.parent_path(), error);
        std::ofstream file(m_pipelineCachePath, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(dataSize));
    }

    void VulkanRHIDevice::CleanupSwapchain()
    {
        for (size_t i = 0; i < m_swapchainImageViews.size(); i++)
        {
            if (m_swapchainImageViews[i] != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_device, m_swapchainImageViews[i], nullptr);
            }
            if (m_swapchainTextures[i].IsValid())
            {
                VulkanTexture* tex = m_textures.Get(m_swapchainTextures[i]);
                if (tex)
                {
                    tex->view = VK_NULL_HANDLE;
                    tex->image = VK_NULL_HANDLE;
                }
                m_textures.Destroy(m_swapchainTextures[i]);
            }
        }

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }

        m_swapchainImageViews.clear();
        m_swapchainImages.clear();
        m_swapchainTextures.clear();
    }

    void VulkanRHIDevice::Resize(uint32_t width, uint32_t height)
    {
        m_width = width;
        m_height = height;

        WaitIdle();

        CleanupSwapchain();
        CreateSwapchain();
    }

} // namespace ChikaEngine::Render
