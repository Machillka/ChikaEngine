#pragma once

#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/rhi/Vulkan/VulkanResource.hpp"
#include "VulkanRHIDevice.hpp"
#include <string_view>
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

        /**
         * @brief 将后端无关诊断接口映射到 Vulkan Debug Utils。
         *
         * 这些接口只增强 GPU 捕获可读性，不影响命令录制结果。
         */
        void SetDebugName(std::string_view name) override;
        void BeginDebugLabel(std::string_view name, const float color[4]) override;
        void EndDebugLabel() override;
        void BeginTimestampScope(std::string_view name) override;
        void EndTimestampScope() override;

        void BeginRendering(const std::vector<RenderingAttachment>& colors, const RenderingAttachment* depth) override;
        void EndRendering() override;

        void BindPipeline(PipelineHandle pipeline) override;
        void BindResources(const ResourceBindingGroup& group) override;

        void InsertTextureBarrier(TextureHandle tex, ResourceState before, ResourceState after, const TextureSubresourceRange& range = {}) override;
        void InsertBufferBarrier(BufferHandle buffer, ResourceState before, ResourceState after, const BufferRange& range = {}) override;

        // push 到 current pipeline 中 ->
        // NOTE: 是否需要提供 Pipeline Handle 来指定绑定？
        void PushConstants(std::string_view rangeName, const void* data, uint32_t size) override;

        void BindVertexBuffer(BufferHandle buffer, uint64_t offset) override;
        void BindIndexBuffer(BufferHandle buffer, uint64_t offset, bool isUint32) override;
        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override;
        void DrawIndirect(BufferHandle arguments, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
        void DrawIndexedIndirect(BufferHandle arguments, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

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
        std::vector<uint32_t> m_timestampEndQueries;
    };
} // namespace ChikaEngine::Render
