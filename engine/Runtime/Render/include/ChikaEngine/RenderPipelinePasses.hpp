#pragma once

#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/RenderGraphBlackboard.hpp"
#include "ChikaEngine/math/mat4.h"

namespace ChikaEngine::Render::PassModules
{
    /** @brief 创建 SceneDepth 的唯一物理/RenderGraph descriptor 契约。 */
    TextureDesc MakeSceneDepthDescription(uint32_t width, uint32_t height);
    /** @brief 生成只保留相机旋转的数学 inverse view-projection，避免平移和 clip plane 扭曲天空方向。 */
    Math::Mat4 MakeSkyboxInverseViewProjection(const Math::Mat4& view, const Math::Mat4& projection);

    struct GBufferDescriptions
    {
        TextureDesc albedo;
        TextureDesc normal;
        TextureDesc material;
        TextureDesc position;
    };

    /** @brief 添加仅写深度的 Shadow Pass。 */
    void AddShadow(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute);
    /** @brief 添加 Skybox Pass；Deferred 路径可选择采样 SceneDepth，但始终不声明 depth attachment。 */
    void AddSkybox(RenderGraph& graph, const RenderGraphBlackboard& blackboard, LoadOp colorLoadOp, const float clearColor[4], bool sampleSceneDepth, RGExecuteCallback execute);
    /** @brief 添加 Forward Scene Pass，并由调用方明确 HDR 是否保留已有 Skybox。 */
    void AddForward(RenderGraph& graph, const RenderGraphBlackboard& blackboard, LoadOp colorLoadOp, const float clearColor[4], RGExecuteCallback execute);
    /** @brief 添加消费 GPU culling buffer 的 Forward Scene Pass。 */
    void AddGpuDrivenForward(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGBufferHandle instances, RGBufferHandle visibleInstances, RGBufferHandle indirectArguments, LoadOp colorLoadOp, const float clearColor[4], RGExecuteCallback execute);
    /** @brief 创建 GBuffer Transient 资源并添加 GBuffer Pass。 */
    void AddGBuffer(RenderGraph& graph, RenderGraphBlackboard& blackboard, const GBufferDescriptions& descriptions, RGExecuteCallback execute);
    /** @brief 添加消费 GBuffer 的 Deferred Lighting Pass。 */
    void AddDeferredLighting(RenderGraph& graph, const RenderGraphBlackboard& blackboard, const float clearColor[4], RGExecuteCallback execute);
    /** @brief 添加保留 Scene Color/Depth 的透明 Pass。 */
    void AddTransparent(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute);
    /** @brief 将线性 HDR Scene Color 转换为供显示和编辑器 Viewport 使用的 LDR Scene Color。 */
    void AddPostProcess(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute);
    /** @brief 添加最终 Overlay Composite Pass，并为外部扩展提供命令录制位置。 */
    void AddOverlay(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute);
} // namespace ChikaEngine::Render::PassModules
