#include "ChikaEngine/rhi/Vulkan/VulkanResource.hpp"
#include <ChikaEngine/rhi/Vulkan/VulkanCommandList.hpp>
#include <ChikaEngine/rhi/Vulkan/VulkanHelper.hpp>

namespace ChikaEngine::Render
{

    VulkanCommandList::VulkanCommandList(VulkanRHIDevice* device, VkCommandBuffer cmd) : m_device(device), m_cmd(cmd), m_currentPSO(nullptr) {}

    VulkanCommandList::~VulkanCommandList() {}

    void VulkanCommandList::Begin()
    {
        VkCommandBufferBeginInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(m_cmd, &info);
    }

    void VulkanCommandList::End()
    {
        vkEndCommandBuffer(m_cmd);
    }

    // TODO[x]: 加入分辨率推导
    void VulkanCommandList::BeginRendering(const std::vector<RenderingAttachment>& colors, const RenderingAttachment* depth)
    {
        std::vector<VkRenderingAttachmentInfo> colorInfos;

        VulkanTexture* vkTex = nullptr;

        if (!colors.empty())
            vkTex = m_device->GetVkTexture(colors[0].texture);
        else if (depth)
            vkTex = m_device->GetVkTexture(depth->texture);

        uint32_t width = 1920, height = 1080;

        if (vkTex)
        {
            width = vkTex->width;
            height = vkTex->height;
        }

        VkRect2D renderArea{ { 0, 0 }, { width, height } };

        for (const auto& att : colors)
        {
            VulkanTexture* vkTex = m_device->GetVkTexture(att.texture);
            VkRenderingAttachmentInfo info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            info.imageView = vkTex->view;
            info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            info.loadOp = (att.loadOp == LoadOp::Clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            info.storeOp = (att.storeOp == StoreOp::Store) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
            info.clearValue.color = { att.clearColor[0], att.clearColor[1], att.clearColor[2], att.clearColor[3] };
            colorInfos.push_back(info);
        }

        VkRenderingAttachmentInfo depthInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        if (depth)
        {
            VulkanTexture* vkTex = m_device->GetVkTexture(depth->texture);
            depthInfo.imageView = vkTex->view;
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthInfo.loadOp = (depth->loadOp == LoadOp::Clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            depthInfo.storeOp = (depth->storeOp == StoreOp::Store) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthInfo.clearValue.depthStencil = { 1.0f, 0 };
        }

        VkRenderingInfo renderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderInfo.renderArea = renderArea;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = colorInfos.size();
        renderInfo.pColorAttachments = colorInfos.data();
        renderInfo.pDepthAttachment = depth ? &depthInfo : nullptr;

        vkCmdBeginRendering(m_cmd, &renderInfo);

        // 设置动态状态 (必须在 BeginRendering 之后，Draw 之前设置)
        VkViewport viewport{ 0.0f, 0.0f, (float)renderArea.extent.width, (float)renderArea.extent.height, 0.0f, 1.0f };
        vkCmdSetViewport(m_cmd, 0, 1, &viewport);
        vkCmdSetScissor(m_cmd, 0, 1, &renderArea);
    }
    void VulkanCommandList::EndRendering()
    {
        vkCmdEndRendering(m_cmd);
    }
    void VulkanCommandList::BindResources(uint32_t setIndex, const ResourceBindingGroup& group)
    {
        VkDescriptorSet vkSet = m_device->AllocateTransientDescriptorSet();

        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorBufferInfo> bInfos(group.buffers.size());
        std::vector<VkDescriptorImageInfo> iInfos(group.textures.size());

        for (size_t i = 0; i < group.buffers.size(); ++i)
        {
            VulkanBuffer* vb = m_device->GetVkBuffer(group.buffers[i].buf);
            bInfos[i] = { vb->buffer, group.buffers[i].offset, group.buffers[i].size };
            VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            w.dstSet = vkSet;
            w.dstBinding = group.buffers[i].slot;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.pBufferInfo = &bInfos[i];
            writes.push_back(w);
        }

        for (size_t i = 0; i < group.textures.size(); ++i)
        {
            VulkanTexture* vt = m_device->GetVkTexture(group.textures[i].tex);
            iInfos[i] = { m_device->GetDefaultSampler(), vt->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            w.dstSet = vkSet;
            w.dstBinding = group.textures[i].slot;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &iInfos[i];
            writes.push_back(w);
        }

        vkUpdateDescriptorSets(m_device->GetRawDevice(), writes.size(), writes.data(), 0, nullptr);
        vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_currentPSO->layout, setIndex, 1, &vkSet, 0, nullptr);
    }

    void VulkanCommandList::BindPipeline(PipelineHandle pipeline)
    {
        m_currentPSO = m_device->GetVkPipeline(pipeline);
        vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_currentPSO->pipeline);
    }

    void VulkanCommandList::PushConstants(uint32_t size, const void* data)
    {
        vkCmdPushConstants(m_cmd, m_currentPSO->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, size, data);
    }
    void VulkanCommandList::BindVertexBuffer(BufferHandle buffer, uint64_t offset)
    {
        VkBuffer b = m_device->GetVkBuffer(buffer)->buffer;
        VkDeviceSize off = offset;
        vkCmdBindVertexBuffers(m_cmd, 0, 1, &b, &off);
    }

    void VulkanCommandList::BindIndexBuffer(BufferHandle buffer, uint64_t offset, bool isUint32)
    {
        VkBuffer b = m_device->GetVkBuffer(buffer)->buffer;
        vkCmdBindIndexBuffer(m_cmd, b, offset, isUint32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    }

    void VulkanCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount)
    {
        vkCmdDrawIndexed(m_cmd, indexCount, instanceCount, 0, 0, 0);
    }

    void VulkanCommandList::CopyBuffer(BufferHandle src, BufferHandle dst, uint64_t size)
    {
        VkBuffer srcBuffer = m_device->GetVkBuffer(src)->buffer;
        VkBuffer dstBuffer = m_device->GetVkBuffer(dst)->buffer;

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;

        vkCmdCopyBuffer(m_cmd, srcBuffer, dstBuffer, 1, &copyRegion);
    }

    void VulkanCommandList::CopyBufferToTexture(BufferHandle src, TextureHandle dst, uint32_t width, uint32_t height)
    {
        VkBuffer srcBuffer = m_device->GetVkBuffer(src)->buffer;
        VkImage dstImage = m_device->GetVkTexture(dst)->image;

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {
            0,
            0,
            0,
        };
        region.imageExtent = {
            width,
            height,
            1,
        };

        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 布局的 texture
        vkCmdCopyBufferToImage(m_cmd, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    void VulkanCommandList::InsertTextureBarrier(TextureHandle tex, ResourceState before, ResourceState after)
    {
        if (after == ResourceState::Undefined)
            return;
        VulkanTexture* vkTex = m_device->GetVkTexture(tex);
        if (!vkTex)
            return;

        VkImageLayout oldLayout, newLayout;
        VkAccessFlags srcA, dstA;
        VkPipelineStageFlags srcS, dstS;
        GetVkStateInfo(before, oldLayout, srcA, srcS);
        GetVkStateInfo(after, newLayout, dstA, dstS);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcAccessMask = srcA;
        barrier.dstAccessMask = dstA;
        barrier.image = vkTex->image;
        bool isDepth = (vkTex->format == VK_FORMAT_D32_SFLOAT || vkTex->format == VK_FORMAT_D24_UNORM_S8_UINT);
        bool hasStencil = (vkTex->format == VK_FORMAT_D24_UNORM_S8_UINT);

        if (isDepth)
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencil)
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        else
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(m_cmd, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

} // namespace ChikaEngine::Render