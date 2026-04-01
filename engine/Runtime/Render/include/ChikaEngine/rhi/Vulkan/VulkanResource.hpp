/*!
 * @file VulkanResource.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 把 RHI 资源通过 Vulkan 实现
 * @version 0.1
 * @date 2026-03-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace ChikaEngine::Render
{
    struct VulkanBuffer
    {
        VkBuffer buffer;
        VmaAllocation allocation; // 替代了 VkDeviceMemory
        VmaAllocationInfo allocInfo;
    };

    struct VulkanShader
    {
        VkShaderModule module;
    };

    struct VulkanPipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };

    struct VulkanTexture
    {
        VkImage image;
        VkImageView view;
        VmaAllocation allocation;
        VkFormat format;

        // 缓存大小 以推导分辨率
        uint32_t width = 0;
        uint32_t height = 0;

        // 缓存 DescriptorSet 可以像 imgui 提供
        void* handle = nullptr;
    };
} // namespace ChikaEngine::Render