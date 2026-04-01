#pragma once

#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/rhi/Vulkan/VulkanResource.hpp"
#include "VulkanRHIDevice.hpp"
#include <vulkan/vulkan.h>

namespace ChikaEngine::Render
{

    class VulkanCommandList : public IRHICommandList
    {
      public:
        VulkanCommandList(VulkanRHIDevice* device, VkCommandBuffer cmd);

        ~VulkanCommandList() override;
        void Begin() override;
        void End() override;

        void BeginRendering(const std::vector<RenderingAttachment>& colors, const RenderingAttachment* depth) override;
        void EndRendering() override;

        void BindPipeline(PipelineHandle pipeline) override;
        void BindResources(uint32_t setIndex, const ResourceBindingGroup& group) override;

        void InsertTextureBarrier(TextureHandle tex, ResourceState before, ResourceState after) override;

        // push 到 current pipeline 中 ->
        // NOTE: 是否需要提供 Pipeline Handle 来指定绑定？
        void PushConstants(uint32_t size, const void* data) override;

        void BindVertexBuffer(BufferHandle buffer, uint64_t offset) override;
        void BindIndexBuffer(BufferHandle buffer, uint64_t offset, bool isUint32) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount) override;

        void CopyBuffer(BufferHandle src, BufferHandle dst, uint64_t size) override;
        void CopyBufferToTexture(BufferHandle src, TextureHandle dst, uint32_t width, uint32_t height) override;

        VkCommandBuffer GetVkCmdRaw() const
        {
            return m_cmd;
        }

      private:
        VulkanRHIDevice* m_device = nullptr;
        VkCommandBuffer m_cmd = VK_NULL_HANDLE;
        VulkanPipeline* m_currentPSO = nullptr;
        bool m_recording = false;
    };
} // namespace ChikaEngine::Render