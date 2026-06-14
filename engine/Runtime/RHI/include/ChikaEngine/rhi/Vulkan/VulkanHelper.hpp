#pragma once

#include "ChikaEngine/RHIDesc.hpp"
#include <vulkan/vulkan.h>
#include <ChikaEngine/debug/Debug.h>
#include <stdexcept>

namespace ChikaEngine::Render
{
    /**
     * @brief 将后端无关 Shader Stage Mask 转换为 Vulkan 标志。
     */
    static VkShaderStageFlags ToVkShaderStages(Shader::ShaderStageMask stages)
    {
        VkShaderStageFlags result = 0;
        if (Shader::HasStage(stages, Shader::ShaderStageMask::Vertex))
            result |= VK_SHADER_STAGE_VERTEX_BIT;
        if (Shader::HasStage(stages, Shader::ShaderStageMask::Fragment))
            result |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (Shader::HasStage(stages, Shader::ShaderStageMask::Compute))
            result |= VK_SHADER_STAGE_COMPUTE_BIT;
        return result;
    }

    /**
     * @brief 将 Reflection Descriptor 类型转换为 Vulkan Descriptor 类型。
     */
    static VkDescriptorType ToVkDescriptorType(Shader::ShaderDescriptorType type)
    {
        switch (type)
        {
        case Shader::ShaderDescriptorType::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case Shader::ShaderDescriptorType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case Shader::ShaderDescriptorType::CombinedImageSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case Shader::ShaderDescriptorType::SampledImage:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case Shader::ShaderDescriptorType::StorageImage:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case Shader::ShaderDescriptorType::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        default:
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

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
        case RHI_Format::RGBA8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
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

    static VkPrimitiveTopology ToVkTopology(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::LineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::TriangleList:
        default:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    static VkCullModeFlags ToVkCullMode(CullMode mode)
    {
        switch (mode)
        {
        case CullMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:
            return VK_CULL_MODE_BACK_BIT;
        case CullMode::None:
        default:
            return VK_CULL_MODE_NONE;
        }
    }

    static VkFrontFace ToVkFrontFace(FrontFace face)
    {
        return face == FrontFace::Clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    static VkCompareOp ToVkCompareOp(CompareOp op)
    {
        switch (op)
        {
        case CompareOp::Never:
            return VK_COMPARE_OP_NEVER;
        case CompareOp::Less:
            return VK_COMPARE_OP_LESS;
        case CompareOp::Greater:
            return VK_COMPARE_OP_GREATER;
        case CompareOp::Always:
            return VK_COMPARE_OP_ALWAYS;
        case CompareOp::LessOrEqual:
        default:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
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

        case ResourceState::DepthRead:
            layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
            access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
            stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        case ResourceState::ShaderResource:
            layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            access = VK_ACCESS_SHADER_READ_BIT;
            stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;

        case ResourceState::StorageRead:
            layout = VK_IMAGE_LAYOUT_GENERAL;
            access = VK_ACCESS_SHADER_READ_BIT;
            stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        case ResourceState::StorageWrite:
            layout = VK_IMAGE_LAYOUT_GENERAL;
            access = VK_ACCESS_SHADER_WRITE_BIT;
            stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        case ResourceState::CopySrc:
            layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            access = VK_ACCESS_TRANSFER_READ_BIT;
            stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case ResourceState::CopyDst:
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

    static VkFilter ToVkFilter(FilterMode mode)
    {
        return mode == FilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    }

    static VkSamplerAddressMode ToVkAddressMode(AddressMode mode)
    {
        switch (mode)
        {
        case AddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case AddressMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case AddressMode::Repeat:
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    static VkSampleCountFlagBits ToVkSampleCount(uint32_t sampleCount)
    {
        switch (sampleCount)
        {
        case 2:
            return VK_SAMPLE_COUNT_2_BIT;
        case 4:
            return VK_SAMPLE_COUNT_4_BIT;
        case 8:
            return VK_SAMPLE_COUNT_8_BIT;
        case 1:
        default:
            return VK_SAMPLE_COUNT_1_BIT;
        }
    }

    /**
     * @brief 将后端无关 Buffer State 转换为 Vulkan Access/Stage。
     */
    static void GetVkBufferStateInfo(ResourceState state, VkAccessFlags& access, VkPipelineStageFlags& stage)
    {
        switch (state)
        {
        case ResourceState::CopySrc:
            access = VK_ACCESS_TRANSFER_READ_BIT;
            stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ResourceState::CopyDst:
            access = VK_ACCESS_TRANSFER_WRITE_BIT;
            stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ResourceState::VertexBuffer:
            access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            break;
        case ResourceState::IndexBuffer:
            access = VK_ACCESS_INDEX_READ_BIT;
            stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            break;
        case ResourceState::UniformBuffer:
            access = VK_ACCESS_UNIFORM_READ_BIT;
            stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case ResourceState::StorageRead:
            access = VK_ACCESS_SHADER_READ_BIT;
            stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case ResourceState::StorageWrite:
            access = VK_ACCESS_SHADER_WRITE_BIT;
            stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case ResourceState::IndirectArgument:
            access = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            stage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            break;
        case ResourceState::Undefined:
        default:
            access = 0;
            stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
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
