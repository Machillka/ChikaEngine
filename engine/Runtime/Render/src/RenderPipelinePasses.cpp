#include "ChikaEngine/RenderPipelinePasses.hpp"

namespace ChikaEngine::Render::PassModules
{
    TextureDesc MakeSceneDepthDescription(uint32_t width, uint32_t height)
    {
        return {
            .width = width,
            .height = height,
            .format = RHI_Format::D32_SFloat,
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = RHI_TextureUsage::DepthStencilAttachment | RHI_TextureUsage::Sampled,
        };
    }

    Math::Mat4 MakeSkyboxInverseViewProjection(const Math::Mat4& view, const Math::Mat4& projection)
    {
        Math::Mat4 rotationOnlyView = view;
        rotationOnlyView(0, 3) = 0.0f;
        rotationOnlyView(1, 3) = 0.0f;
        rotationOnlyView(2, 3) = 0.0f;

        // Mat4::Inverse() 当前返回适合直接列主序上传的转置结果；SkyboxData 在上传前还会统一转置一次，
        // 因此这里恢复数学意义上的 inverse，避免 Shader 最终收到 inverse-transpose 并让边缘射线依赖 near clip。
        return (projection * rotationOnlyView).Inverse().Transposed();
    }

    void AddShadow(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGExecuteCallback execute)
    {
        graph.AddPass("Shadow Pass", [&](RGPassBuilder& builder) { builder.WriteDepth(blackboard.GetTexture(RenderGraphSemantic::ShadowDepth), LoadOp::Clear); }, std::move(execute));
    }

    void AddSkybox(RenderGraph& graph, const RenderGraphBlackboard& blackboard, LoadOp colorLoadOp, const float clearColor[4], bool sampleSceneDepth, RGExecuteCallback execute)
    {
        graph.AddPass(
            "Skybox Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::EnvironmentSkybox));
                if (sampleSceneDepth)
                    builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::SceneDepth));
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::HDRSceneColor), colorLoadOp, clearColor);
            },
            std::move(execute));
    }

    void AddForward(RenderGraph& graph, const RenderGraphBlackboard& blackboard, LoadOp colorLoadOp, const float clearColor[4], RGExecuteCallback execute)
    {
        graph.AddPass(
            "Main Scene Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::ShadowDepth));
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::HDRSceneColor), colorLoadOp, clearColor);
                builder.WriteDepth(blackboard.GetTexture(RenderGraphSemantic::SceneDepth), LoadOp::Clear);
            },
            std::move(execute));
    }

    void AddGpuDrivenForward(RenderGraph& graph, const RenderGraphBlackboard& blackboard, RGBufferHandle instances, RGBufferHandle visibleInstances, RGBufferHandle indirectArguments, LoadOp colorLoadOp, const float clearColor[4], RGExecuteCallback execute)
    {
        graph.AddPass(
            "GPU Driven Main Scene Pass",
            [&](RGPassBuilder& builder)
            {
                builder.ReadTexture(blackboard.GetTexture(RenderGraphSemantic::ShadowDepth));
                builder.ReadBuffer(instances, ResourceState::StorageRead);
                builder.ReadBuffer(visibleInstances, ResourceState::StorageRead);
                builder.ReadBuffer(indirectArguments, ResourceState::IndirectArgument);
                builder.WriteColor(blackboard.GetTexture(RenderGraphSemantic::HDRSceneColor), colorLoadOp, clearColor);
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

    void AddDeferredLighting(RenderGraph& graph, const RenderGraphBlackboard& blackboard, const float clearColor[4], RGExecuteCallback execute)
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
