#include "ChikaEngine/RenderPipelinePasses.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    namespace Render = ChikaEngine::Render;

    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    struct GraphFixture
    {
        GraphFixture()
            : hdr(graph.ImportTexture("HDRSceneColor", NextTexture(), ColorDescription(Render::RHI_Format::RGBA16_Float))), ldr(graph.ImportTexture("SceneColor", NextTexture(), ColorDescription(Render::RHI_Format::RGBA8_UNorm))), depth(graph.ImportTexture("SceneDepth", NextTexture(), Render::PassModules::MakeSceneDepthDescription(128, 72))), shadow(graph.ImportTexture("ShadowDepth", NextTexture(), Render::PassModules::MakeSceneDepthDescription(16, 16))),
              environment(graph.ImportTexture("Environment.Skybox", NextTexture(), EnvironmentDescription()))
        {
            blackboard.SetTexture(std::string(Render::RenderGraphSemantic::HDRSceneColor), hdr);
            blackboard.SetTexture(std::string(Render::RenderGraphSemantic::SceneColor), ldr);
            blackboard.SetTexture(std::string(Render::RenderGraphSemantic::SceneDepth), depth);
            blackboard.SetTexture(std::string(Render::RenderGraphSemantic::ShadowDepth), shadow);
            blackboard.SetTexture(std::string(Render::RenderGraphSemantic::EnvironmentSkybox), environment);
        }

        static Render::TextureHandle NextTexture()
        {
            static uint32_t next = 1;
            return Render::TextureHandle::FromParts(next++, 1);
        }

        static Render::BufferHandle NextBuffer()
        {
            static uint32_t next = 1;
            return Render::BufferHandle::FromParts(next++, 1);
        }

        static Render::TextureDesc ColorDescription(Render::RHI_Format format)
        {
            return {
                .width = 128,
                .height = 72,
                .format = format,
                .usage = Render::RHI_TextureUsage::ColorAttachment | Render::RHI_TextureUsage::Sampled,
            };
        }

        static Render::TextureDesc EnvironmentDescription()
        {
            return {
                .width = 16,
                .height = 16,
                .format = Render::RHI_Format::RGBA16_Float,
                .mipLevels = 1,
                .arrayLayers = 6,
                .usage = Render::RHI_TextureUsage::Sampled,
                .dimension = Render::TextureDimension::TextureCube,
            };
        }

        Render::RGBufferHandle ImportBuffer(std::string name, Render::ResourceState finalState)
        {
            const Render::BufferDesc desc{
                .size = 64,
                .usage = Render::RHI_BufferUsage::Storage | Render::RHI_BufferUsage::Indirect,
            };
            return graph.ImportBuffer(std::move(name), NextBuffer(), desc, finalState, finalState);
        }

        void AddOutput()
        {
            Render::PassModules::AddPostProcess(graph, blackboard, [](Render::IRHICommandList*, Render::RenderGraph*) {});
            graph.AddPresentPass("Present", ldr);
        }

        Render::RenderGraph graph{ nullptr };
        Render::RenderGraphBlackboard blackboard;
        Render::RGTextureHandle hdr;
        Render::RGTextureHandle ldr;
        Render::RGTextureHandle depth;
        Render::RGTextureHandle shadow;
        Render::RGTextureHandle environment;
    };

    const Render::RenderGraphPassDebugInfo* FindPass(const Render::RenderGraphDebugSnapshot& snapshot, const std::string& name)
    {
        const auto found = std::ranges::find(snapshot.passes, name, &Render::RenderGraphPassDebugInfo::name);
        return found == snapshot.passes.end() ? nullptr : &*found;
    }

    bool HasRead(const Render::RenderGraphPassDebugInfo& pass, const std::string& resource, Render::ResourceState state)
    {
        return std::ranges::any_of(pass.textureReads, [&](const Render::RenderGraphTextureAccessDebugInfo& access) { return access.resource == resource && access.state == state; });
    }

    Render::LoadOp ColorLoadOp(const Render::RenderGraphPassDebugInfo& pass)
    {
        return pass.colorWrites.empty() ? Render::LoadOp::Load : pass.colorWrites.front().loadOp;
    }

    void TestForwardOrdering(bool gpuDriven)
    {
        GraphFixture fixture;
        const float fallback[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
        Render::PassModules::AddSkybox(fixture.graph, fixture.blackboard, Render::LoadOp::Clear, fallback, false, [](Render::IRHICommandList*, Render::RenderGraph*) {});
        const std::string sceneName = gpuDriven ? "GPU Driven Main Scene Pass" : "Main Scene Pass";
        if (gpuDriven)
        {
            const Render::RGBufferHandle instances = fixture.ImportBuffer("Instances", Render::ResourceState::StorageRead);
            const Render::RGBufferHandle visible = fixture.ImportBuffer("Visible", Render::ResourceState::StorageRead);
            const Render::RGBufferHandle indirect = fixture.ImportBuffer("Indirect", Render::ResourceState::IndirectArgument);
            Render::PassModules::AddGpuDrivenForward(fixture.graph, fixture.blackboard, instances, visible, indirect, Render::LoadOp::Load, fallback, [](Render::IRHICommandList*, Render::RenderGraph*) {});
        }
        else
        {
            Render::PassModules::AddForward(fixture.graph, fixture.blackboard, Render::LoadOp::Load, fallback, [](Render::IRHICommandList*, Render::RenderGraph*) {});
        }
        fixture.AddOutput();

        Check(fixture.graph.Compile(), gpuDriven ? "GPU forward Skybox graph compiles" : "CPU forward Skybox graph compiles");
        const std::vector<std::string> names = fixture.graph.GetCompiledPassNames();
        const auto skybox = std::ranges::find(names, "Skybox Pass");
        const auto scene = std::ranges::find(names, sceneName);
        const auto post = std::ranges::find(names, "Post Process Composite");
        Check(skybox < scene && scene < post, gpuDriven ? "GPU forward orders Skybox before scene and post" : "CPU forward orders Skybox before scene and post");

        const Render::RenderGraphDebugSnapshot& snapshot = fixture.graph.GetDebugSnapshot();
        const Render::RenderGraphPassDebugInfo* skyboxInfo = FindPass(snapshot, "Skybox Pass");
        const Render::RenderGraphPassDebugInfo* sceneInfo = FindPass(snapshot, sceneName);
        Check(skyboxInfo && ColorLoadOp(*skyboxInfo) == Render::LoadOp::Clear, "forward Skybox clears HDR");
        Check(sceneInfo && ColorLoadOp(*sceneInfo) == Render::LoadOp::Load, "forward scene preserves Skybox HDR");
        Check(skyboxInfo && skyboxInfo->depthAttachment.empty() && !HasRead(*skyboxInfo, "SceneDepth", Render::ResourceState::ShaderResource), "forward Skybox neither samples nor writes depth");
    }

    void TestForwardFallbackClear()
    {
        GraphFixture fixture;
        const float fallback[4] = { 0.25f, 0.1f, 0.05f, 1.0f };
        Render::PassModules::AddForward(fixture.graph, fixture.blackboard, Render::LoadOp::Clear, fallback, [](Render::IRHICommandList*, Render::RenderGraph*) {});
        fixture.AddOutput();
        Check(fixture.graph.Compile(), "forward fallback graph compiles without a Skybox pass");
        Check(FindPass(fixture.graph.GetDebugSnapshot(), "Skybox Pass") == nullptr, "disabled or invalid Skybox creates no pass");
        const Render::RenderGraphPassDebugInfo* scene = FindPass(fixture.graph.GetDebugSnapshot(), "Main Scene Pass");
        Check(scene && ColorLoadOp(*scene) == Render::LoadOp::Clear, "scene explicitly clears HDR when Skybox is absent");
    }

    void AddDeferredGBuffer(GraphFixture& fixture, bool gpuDriven)
    {
        Render::PassModules::GBufferDescriptions descriptions{
            .albedo = GraphFixture::ColorDescription(Render::RHI_Format::RGBA8_UNorm),
            .normal = GraphFixture::ColorDescription(Render::RHI_Format::RGBA16_Float),
            .material = GraphFixture::ColorDescription(Render::RHI_Format::RGBA16_Float),
            .position = GraphFixture::ColorDescription(Render::RHI_Format::RGBA16_Float),
        };
        if (!gpuDriven)
        {
            Render::PassModules::AddGBuffer(fixture.graph, fixture.blackboard, descriptions, [](Render::IRHICommandList*, Render::RenderGraph*) {});
            return;
        }

        fixture.blackboard.SetTexture(std::string(Render::RenderGraphSemantic::GBufferAlbedo), fixture.graph._RegisterTexture(std::string(Render::RenderGraphSemantic::GBufferAlbedo), descriptions.albedo));
        fixture.blackboard.SetTexture(std::string(Render::RenderGraphSemantic::GBufferNormal), fixture.graph._RegisterTexture(std::string(Render::RenderGraphSemantic::GBufferNormal), descriptions.normal));
        fixture.blackboard.SetTexture(std::string(Render::RenderGraphSemantic::GBufferMaterial), fixture.graph._RegisterTexture(std::string(Render::RenderGraphSemantic::GBufferMaterial), descriptions.material));
        fixture.blackboard.SetTexture(std::string(Render::RenderGraphSemantic::GBufferPosition), fixture.graph._RegisterTexture(std::string(Render::RenderGraphSemantic::GBufferPosition), descriptions.position));
        fixture.graph.AddPass(
            "GPU Driven Deferred GBuffer Pass",
            [&](Render::RGPassBuilder& builder)
            {
                const float clear[4] = {};
                builder.WriteColor(fixture.blackboard.GetTexture(Render::RenderGraphSemantic::GBufferAlbedo), Render::LoadOp::Clear, clear);
                builder.WriteColor(fixture.blackboard.GetTexture(Render::RenderGraphSemantic::GBufferNormal), Render::LoadOp::Clear, clear);
                builder.WriteColor(fixture.blackboard.GetTexture(Render::RenderGraphSemantic::GBufferMaterial), Render::LoadOp::Clear, clear);
                builder.WriteColor(fixture.blackboard.GetTexture(Render::RenderGraphSemantic::GBufferPosition), Render::LoadOp::Clear, clear);
                builder.WriteDepth(fixture.depth, Render::LoadOp::Clear);
            },
            [](Render::IRHICommandList*, Render::RenderGraph*) {});
    }

    void TestDeferredOrdering(bool gpuDriven)
    {
        GraphFixture fixture;
        const float fallback[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
        AddDeferredGBuffer(fixture, gpuDriven);
        Render::PassModules::AddDeferredLighting(fixture.graph, fixture.blackboard, fallback, [](Render::IRHICommandList*, Render::RenderGraph*) {});
        Render::PassModules::AddSkybox(fixture.graph, fixture.blackboard, Render::LoadOp::Load, fallback, true, [](Render::IRHICommandList*, Render::RenderGraph*) {});
        Render::PassModules::AddTransparent(fixture.graph, fixture.blackboard, [](Render::IRHICommandList*, Render::RenderGraph*) {});
        fixture.AddOutput();

        Check(fixture.graph.Compile(), gpuDriven ? "GPU deferred Skybox graph compiles" : "CPU deferred Skybox graph compiles");
        const std::vector<std::string> names = fixture.graph.GetCompiledPassNames();
        const std::string gbufferName = gpuDriven ? "GPU Driven Deferred GBuffer Pass" : "Deferred GBuffer Pass";
        Check(std::ranges::find(names, gbufferName) < std::ranges::find(names, "Deferred Lighting Pass") && std::ranges::find(names, "Deferred Lighting Pass") < std::ranges::find(names, "Skybox Pass") && std::ranges::find(names, "Skybox Pass") < std::ranges::find(names, "Forward Transparent Pass") && std::ranges::find(names, "Forward Transparent Pass") < std::ranges::find(names, "Post Process Composite"),
              gpuDriven ? "GPU deferred order is GBuffer, lighting, Skybox, transparent, post" : "CPU deferred order is GBuffer, lighting, Skybox, transparent, post");

        const Render::RenderGraphDebugSnapshot& snapshot = fixture.graph.GetDebugSnapshot();
        const Render::RenderGraphPassDebugInfo* lighting = FindPass(snapshot, "Deferred Lighting Pass");
        const Render::RenderGraphPassDebugInfo* skybox = FindPass(snapshot, "Skybox Pass");
        Check(lighting && ColorLoadOp(*lighting) == Render::LoadOp::Clear, "deferred lighting initializes HDR with fallback color");
        Check(skybox && ColorLoadOp(*skybox) == Render::LoadOp::Load, "deferred Skybox preserves lit HDR");
        Check(skybox && HasRead(*skybox, "SceneDepth", Render::ResourceState::ShaderResource), "deferred Skybox samples SceneDepth");
        Check(skybox && HasRead(*skybox, "Environment.Skybox", Render::ResourceState::ShaderResource), "Skybox pass samples the environment Cubemap");
        Check(skybox && skybox->depthAttachment.empty(), "deferred Skybox does not declare a writable depth attachment");
    }

    void TestSceneDepthDescription()
    {
        const Render::TextureDesc first = Render::PassModules::MakeSceneDepthDescription(1280, 720);
        const Render::TextureDesc resized = Render::PassModules::MakeSceneDepthDescription(1920, 1080);
        Check(first.width == 1280 && first.height == 720, "initial SceneDepth dimensions are preserved");
        Check(resized.width == 1920 && resized.height == 1080, "resized SceneDepth dimensions are preserved");
        Check((first.usage & Render::RHI_TextureUsage::DepthStencilAttachment) != Render::RHI_TextureUsage::None && (first.usage & Render::RHI_TextureUsage::Sampled) != Render::RHI_TextureUsage::None, "SceneDepth supports attachment writes and shader sampling");
    }

    void TestFloatEnvironmentMetadata()
    {
        GraphFixture fixture;
        const Render::TextureDesc& environment = fixture.graph.GetTextureDesc(fixture.blackboard.GetTexture(Render::RenderGraphSemantic::EnvironmentSkybox));
        Check(environment.format == Render::RHI_Format::RGBA16_Float && environment.dimension == Render::TextureDimension::TextureCube && environment.arrayLayers == 6 && environment.mipLevels == 1, "Environment.Skybox preserves float Cubemap metadata in the Blackboard");
    }

    bool MatricesEqual(const ChikaEngine::Math::Mat4& lhs, const ChikaEngine::Math::Mat4& rhs)
    {
        for (size_t index = 0; index < lhs.m.size(); ++index)
        {
            if (std::abs(lhs.m[index] - rhs.m[index]) > 0.00001f)
                return false;
        }
        return true;
    }

    ChikaEngine::Math::Vector3 ReconstructSkyboxDirection(const ChikaEngine::Math::Mat4& inverseViewProjection, float ndcX, float ndcY)
    {
        const ChikaEngine::Math::Vector4 worldPosition = inverseViewProjection * ChikaEngine::Math::Vector4(ndcX, ndcY, 1.0f, 1.0f);
        return ChikaEngine::Math::Vector3(worldPosition.x, worldPosition.y, worldPosition.z).Normalized();
    }

    bool DirectionsEqual(const ChikaEngine::Math::Vector3& lhs, const ChikaEngine::Math::Vector3& rhs)
    {
        return ChikaEngine::Math::Vector3::Dot(lhs, rhs) > 0.99999f;
    }

    void TestTranslationInvariantSkyboxMatrix()
    {
        using ChikaEngine::Math::Mat4;
        using ChikaEngine::Math::Vector3;
        constexpr float fovRadians = 60.0f * 0.01745329251994329577f;
        Mat4 projection = Mat4::Perspective(fovRadians, 16.0f / 9.0f, 0.1f, 1000.0f);
        projection(1, 1) *= -1.0f;
        const Mat4 firstView = Mat4::LookAt({ 0.0f, 1.0f, 4.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
        const Mat4 translatedView = Mat4::LookAt({ 10.0f, 3.0f, -2.0f }, { 10.0f, 3.0f, -6.0f }, { 0.0f, 1.0f, 0.0f });
        const Mat4 rotatedView = Mat4::LookAt({ 0.0f, 1.0f, 4.0f }, { 4.0f, 1.0f, 4.0f }, { 0.0f, 1.0f, 0.0f });

        const Mat4 first = Render::PassModules::MakeSkyboxInverseViewProjection(firstView, projection);
        const Mat4 translated = Render::PassModules::MakeSkyboxInverseViewProjection(translatedView, projection);
        const Mat4 rotated = Render::PassModules::MakeSkyboxInverseViewProjection(rotatedView, projection);
        Check(MatricesEqual(first, translated), "Skybox reconstruction ignores camera translation");
        Check(!MatricesEqual(first, rotated), "Skybox reconstruction responds to camera rotation");
    }

    void TestSkyboxRayDoesNotDependOnClipPlanes()
    {
        using ChikaEngine::Math::Mat4;
        using ChikaEngine::Math::Vector3;
        constexpr float fovRadians = 60.0f * 0.01745329251994329577f;
        constexpr float aspect = 16.0f / 9.0f;
        constexpr float ndcX = 0.75f;
        constexpr float ndcY = -0.5f;

        const Mat4 view = Mat4::LookAt({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f });
        Mat4 shortNearProjection = Mat4::Perspective(fovRadians, aspect, 0.1f, 1000.0f);
        Mat4 longNearProjection = Mat4::Perspective(fovRadians, aspect, 5.0f, 1000.0f);
        shortNearProjection(1, 1) *= -1.0f;
        longNearProjection(1, 1) *= -1.0f;

        const Vector3 shortNearDirection = ReconstructSkyboxDirection(Render::PassModules::MakeSkyboxInverseViewProjection(view, shortNearProjection), ndcX, ndcY);
        const Vector3 longNearDirection = ReconstructSkyboxDirection(Render::PassModules::MakeSkyboxInverseViewProjection(view, longNearProjection), ndcX, ndcY);
        const Vector3 expectedDirection = Vector3(ndcX / shortNearProjection(0, 0), ndcY / shortNearProjection(1, 1), -1.0f).Normalized();

        Check(DirectionsEqual(shortNearDirection, expectedDirection), "Skybox edge ray follows FOV and aspect instead of clip-plane terms");
        Check(DirectionsEqual(shortNearDirection, longNearDirection), "Skybox ray is invariant to near/far clip changes");
    }
} // namespace

int main()
{
    TestForwardOrdering(false);
    TestForwardOrdering(true);
    TestForwardFallbackClear();
    TestDeferredOrdering(false);
    TestDeferredOrdering(true);
    TestSceneDepthDescription();
    TestFloatEnvironmentMetadata();
    TestTranslationInvariantSkyboxMatrix();
    TestSkyboxRayDoesNotDependOnClipPlanes();

    if (g_failures == 0)
        std::cout << "Skybox RenderGraph pass checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
