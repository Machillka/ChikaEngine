#pragma once

#include "ChikaEngine/RHIDesc.hpp"
#include <vulkan/vulkan.h>
#include <ChikaEngine/debug/Debug.h>
#include <stdexcept>

namespace ChikaEngine::Render
{
    // 单独对 Vulkan Check 的检查封装
    // TODO: 接入 Chika Log System
    static void VK_CHECK(VkResult res, const char* msg)
    {
        if (res != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan", msg);
            throw std::runtime_error(msg);
        }
    }

    /*!
     * @brief  把 RHI 枚举转化成 Vk
     *
     * @param  fmt
     * @return VkFormat
     * @author Machillka (machillka2007@gmail.com)
     * @date 2026-03-13
     */
    static VkFormat ToVkFormat(RHI_Format fmt)
    {
        switch (fmt)
        {
        case RHI_Format::RGBA8_UNorm:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case RHI_Format::BGRA8_UNorm:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case RHI_Format::RGBA16_Float:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RHI_Format::RGBA32_Float:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case RHI_Format::R32_Float:
            return VK_FORMAT_R32_SFLOAT;
        case RHI_Format::D24S8:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case RHI_Format::D32_SFloat:
            return VK_FORMAT_D32_SFLOAT;
        case RHI_Format::RGB32_Float:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case RHI_Format::RG32_Float:
            return VK_FORMAT_R32G32_SFLOAT;
        case RHI_Format::RGBA32_SInt:
            return VK_FORMAT_R32G32B32A32_SINT;
        default:
            return VK_FORMAT_UNDEFINED;
        }
    }

    static void GetVkStateInfo(ResourceState state, VkImageLayout& layout, VkAccessFlags& access, VkPipelineStageFlags& stage)
    {
        switch (state)
        {
        case ResourceState::Undefined:
            layout = VK_IMAGE_LAYOUT_UNDEFINED;
            access = 0;
            stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;

        case ResourceState::RenderTarget:
            layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;

        case ResourceState::DepthWrite:
            layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            break;

        case ResourceState::ShaderResource:
            layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            access = VK_ACCESS_SHADER_READ_BIT;
            stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        case ResourceState::TransferDst:
            layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            access = VK_ACCESS_TRANSFER_WRITE_BIT;
            stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case ResourceState::Present:
            layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            access = 0;
            stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;

        default:
            layout = VK_IMAGE_LAYOUT_UNDEFINED;
            access = 0;
            stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;
        }
    }

    static VkImageUsageFlags ToVkUsage(RHI_TextureUsage usage, RHI_Format format)
    {
        VkImageUsageFlags flags = 0;

        if ((usage & RHI_TextureUsage::Sampled) != RHI_TextureUsage::None)
            flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if ((usage & RHI_TextureUsage::Storage) != RHI_TextureUsage::None)
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;

        if ((usage & RHI_TextureUsage::ColorAttachment) != RHI_TextureUsage::None)
            flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if ((usage & RHI_TextureUsage::DepthStencilAttachment) != RHI_TextureUsage::None)
            flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        if ((usage & RHI_TextureUsage::Presentable) != RHI_TextureUsage::None)
            flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // swapchain images

        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        return flags;
    }

} // namespace ChikaEngine::Render