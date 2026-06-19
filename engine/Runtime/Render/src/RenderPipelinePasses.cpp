#include "ChikaEngine/RenderPipelinePasses.hpp"

namespace ChikaEngine::Render::PassModules
{
    void AddShadow(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute)
    {
        graph.AddPass("Shadow Pass", [&](RGPassBuilder& builder) { builder.WriteDepth(blackboard.GetTexture(RenderGraphSemantic::ShadowDepth), LoadOp::Clear); }, std::move(execute));
    }

    void AddForward(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute)
    {
        graph.AddPass(
            "Main Scene Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::ShadowDepth));
                const float clearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::HDRSceneColor), LoadOp::Clear, clearColor);
                builder.WriteDepth(blackboard.GetTexture(RenderGraphSemantic::SceneDepth), LoadOp::Clear);
            },
            std::move(execute));
    }

    void AddGBuffer(RenderGraph& graph, RenderGraphBlackboard& blackboard, const GBufferDescriptions& descriptions, RGExecuteCallback execute)
    {
        blackboard.SetTexture(std::string(RenderGraphSemantic::GBufferAlbedo), graph._RegisterTexture(std::string(RenderGraphSemantic::GBufferAlbedo), descriptions.albedo));
        blackboard.SetTexture(std::string(RenderGraphSemantic::GBufferNormal), graph._RegisterTexture(std::string(RenderGraphSemantic::GBufferNormal), descriptions.normal));
        blackboard.SetTexture(std::string(RenderGraphSemantic::GBufferMaterial), graph._RegisterTexture(std::string(RenderGraphSemantic::GBufferMaterial), descriptions.material));
        blackboard.SetTexture(std::string(RenderGraphSemantic::GBufferPosition), graph._RegisterTexture(std::string(RenderGraphSemantic::GBufferPosition), descriptions.position));
        graph.AddPass(
            "Deferred GBuffer Pass",
            [&](RGPassBuilder& builder)
            {
                const float clearColor[4] = { 0.02f, 0.02f, 0.02f, 1.0f };
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::GBufferAlbedo), LoadOp::Clear, clearColor);
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::GBufferNormal), LoadOp::Clear, clearColor);
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::GBufferMaterial), LoadOp::Clear, clearColor);
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::GBufferPosition), LoadOp::Clear, clearColor);
                builder.WriteDepth(blackboard.GetTexture(RenderGraphSemantic::SceneDepth), LoadOp::Clear);
            },
            std::move(execute));
    }

    void AddDeferredLighting(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute)
    {
        graph.AddPass(
            "Deferred Lighting Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::GBufferAlbedo));
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::GBufferNormal));
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::GBufferMaterial));
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::GBufferPosition));
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::ShadowDepth));
                const float clearColor[4] = { 0.03f, 0.03f, 0.03f, 1.0f };
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::HDRSceneColor), LoadOp::Clear, clearColor);
            },
            std::move(execute));
    }

    void AddTransparent(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute)
    {
        graph.AddPass(
            "Forward Transparent Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::ShadowDepth));
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::HDRSceneColor), LoadOp::Load);
                builder.WriteDepth(blackboard.GetTexture(RenderGraphSemantic::SceneDepth), LoadOp::Load);
            },
            std::move(execute));
    }

    void AddPostProcess(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute)
    {
        graph.AddPass(
            "Post Process Composite",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::HDRSceneColor));
                const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::SceneColor), LoadOp::Clear, clearColor);
            },
            std::move(execute));
    }

    void AddOverlay(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute)
    {
        graph.AddPass(
            "Overlay Composite Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::SceneColor));
                const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::Swapchain), LoadOp::Clear, clearColor);
            },
            std::move(execute));
    }
} // namespace ChikaEngine::Render::PassModules
