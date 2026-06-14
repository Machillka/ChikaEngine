#pragma once

#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/RenderGraphBlackboard.hpp"

namespace ChikaEngine::Render::PassModules
{
    struct GBufferDescriptions
    {
        TextureDesc albedo;
        TextureDesc normal;
        TextureDesc material;
    };

    /** @brief 添加仅写深度的 Shadow Pass。 */
    void AddShadow(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute);
    /** @brief 添加 Forward Scene Pass。 */
    void AddForward(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute);
    /** @brief 创建 GBuffer Transient 资源并添加 GBuffer Pass。 */
    void AddGBuffer(RenderGraph& graph, RenderGraphBlackboard& blackboard, const GBufferDescriptions& descriptions, RGExecuteCallback execute);
    /** @brief 添加消费 GBuffer 的 Deferred Lighting Pass。 */
    void AddDeferredLighting(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute);
    /** @brief 添加保留 Scene Color/Depth 的透明 Pass。 */
    void AddTransparent(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute);
    /** @brief 添加最终 UI Composite Pass。 */
    void AddUI(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute);
} // namespace ChikaEngine::Render::PassModules
