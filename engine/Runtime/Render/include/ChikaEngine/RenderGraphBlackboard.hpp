#pragma once

#include "ChikaEngine/RenderGraphHandle.hpp"
#include <string>
#include <string_view>
#include <unordered_map>

namespace ChikaEngine::Render
{
    /**
     * @brief 用具名渲染语义连接独立 Pass Module。
     *
     * Blackboard 只保存当前 RenderGraph 的逻辑 Handle，不拥有物理资源，也不能跨 Clear 复用。
     */
    class RenderGraphBlackboard
    {
      public:
        /** @brief 发布具名 Texture 语义，供后续 Pass Module 查询。 */
        void SetTexture(std::string name, RGTextureHandle handle)
        {
            m_textures.insert_or_assign(std::move(name), handle);
        }

        /** @brief 发布具名 Buffer 语义，供后续 Pass Module 查询。 */
        void SetBuffer(std::string name, RGBufferHandle handle)
        {
            m_buffers.insert_or_assign(std::move(name), handle);
        }

        /** @brief 查询 Texture 语义；不存在时返回 Invalid Handle，由 Compile 报告错误。 */
        RGTextureHandle GetTexture(std::string_view name) const
        {
            const auto found = m_textures.find(std::string(name));
            return found == m_textures.end() ? RGTextureHandle::Invalid() : found->second;
        }

        /** @brief 查询 Buffer 语义；不存在时返回 Invalid Handle，由 Compile 报告错误。 */
        RGBufferHandle GetBuffer(std::string_view name) const
        {
            const auto found = m_buffers.find(std::string(name));
            return found == m_buffers.end() ? RGBufferHandle::Invalid() : found->second;
        }

        /** @brief 清除当前帧语义，防止 Handle 跨 RenderGraph::Clear 泄漏。 */
        void Clear()
        {
            m_textures.clear();
            m_buffers.clear();
        }

      private:
        std::unordered_map<std::string, RGTextureHandle> m_textures;
        std::unordered_map<std::string, RGBufferHandle> m_buffers;
    };

    namespace RenderGraphSemantic
    {
        inline constexpr std::string_view HDRSceneColor = "SceneColor.HDR";
        inline constexpr std::string_view SceneColor = "SceneColor";
        inline constexpr std::string_view SceneDepth = "SceneDepth";
        inline constexpr std::string_view ShadowDepth = "ShadowDepth";
        inline constexpr std::string_view Swapchain = "Swapchain";
        inline constexpr std::string_view GBufferAlbedo = "GBuffer.Albedo";
        inline constexpr std::string_view GBufferNormal = "GBuffer.Normal";
        inline constexpr std::string_view GBufferMaterial = "GBuffer.Material";
        inline constexpr std::string_view GBufferPosition = "GBuffer.Position";
    } // namespace RenderGraphSemantic
} // namespace ChikaEngine::Render
