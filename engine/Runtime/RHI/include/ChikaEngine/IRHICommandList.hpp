/*!
 * @file RHICommandList.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief  抽象命令列表接口， Command buffer 功能，GPU 指令缓冲区
 * @version 0.1
 * @date 2026-03-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "RHIDesc.hpp"
#include "RHIResourceHandle.hpp"
#include "ResourceBinder.hpp"
#include <cstdint>
#include <string_view>
#include <vector>
namespace ChikaEngine::Render
{

    class IRHICommandList
    {
      public:
        virtual ~IRHICommandList() = default;

        // Begin/End 命令录制
        virtual void Begin() = 0;
        virtual void End() = 0;

        /**
         * @brief 设置命令列表调试名称，便于在 GPU 调试工具中定位整帧录制入口。
         */
        virtual void SetDebugName(std::string_view name) = 0;

        /**
         * @brief 标记一段具名 GPU 命令区域。
         *
         * RenderGraph 使用该接口标记每个 Pass，使 Validation Layer 和 RenderDoc
         * 可以把底层命令关联回上层 Pass。
         */
        virtual void BeginDebugLabel(std::string_view name, const float color[4]) = 0;
        virtual void EndDebugLabel() = 0;

        virtual void BeginRendering(const std::vector<RenderingAttachment>& colors, const RenderingAttachment* depth) = 0;
        virtual void EndRendering() = 0;

        // 上层构建图的时候调用
        virtual void InsertTextureBarrier(TextureHandle tex, ResourceState before, ResourceState after) = 0;

        virtual void BindPipeline(PipelineHandle pipeline) = 0;
        virtual void BindResources(const ResourceBindingGroup& group) = 0;
        virtual void BindVertexBuffer(BufferHandle buffer, uint64_t offset) = 0;
        virtual void BindIndexBuffer(BufferHandle buffer, uint64_t offset, bool isUint32) = 0;
        virtual void PushConstants(std::string_view rangeName, const void* data, uint32_t size) = 0;

        // 提供绑定指令
        virtual void CopyBuffer(BufferHandle src, BufferHandle dst, uint64_t size) = 0;
        virtual void CopyBufferToTexture(BufferHandle src, TextureHandle dst, uint32_t width, uint32_t height) = 0;

        // 绘制命令
        virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) = 0;

        // imgui 特化
        virtual void DrawImGui(void* drawData) = 0;
    };
} // namespace ChikaEngine::Render
