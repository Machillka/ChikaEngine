#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/EnvironmentResources.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RenderGraph.hpp"
#include "ChikaEngine/RenderGraphBlackboard.hpp"
#include "ChikaEngine/ResourceLayout.hpp"
#include "ChikaEngine/ResourceManager.hpp"
#include "ChikaEngine/RenderSettings.hpp"
#include "ChikaEngine/TextureLoader.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    /** @brief 为 Asset -> Resource 环境贴图集成测试提供最小 CPU 内存 RHI。 */
    class EnvironmentTestRHI final : public ChikaEngine::Render::IRHIDevice
    {
      public:
        void Initialize(const ChikaEngine::Render::RHI_InitParams&) override {}
        void Shutdown() override {}
        void BeginFrame() override {}
        void EndFrame() override {}

        ChikaEngine::Render::BufferHandle CreateBuffer(const ChikaEngine::Render::BufferDesc& desc) override
        {
            const auto handle = ChikaEngine::Render::BufferHandle::FromParts(m_nextHandle++, 1);
            m_bufferData[handle].resize(static_cast<size_t>(desc.size));
            return handle;
        }

        ChikaEngine::Render::TextureHandle CreateTexture(const ChikaEngine::Render::TextureDesc&) override
        {
            return ChikaEngine::Render::TextureHandle::FromParts(m_nextHandle++, 1);
        }

        ChikaEngine::Render::SamplerHandle CreateSampler(const ChikaEngine::Render::SamplerDesc&) override
        {
            return ChikaEngine::Render::SamplerHandle::FromParts(m_nextHandle++, 1);
        }

        ChikaEngine::Render::TextureViewHandle CreateTextureView(const ChikaEngine::Render::TextureViewDesc&) override
        {
            return ChikaEngine::Render::TextureViewHandle::FromParts(m_nextHandle++, 1);
        }

        ChikaEngine::Render::ShaderHandle CreateShader(const ChikaEngine::Render::ShaderDesc&) override
        {
            return ChikaEngine::Render::ShaderHandle::FromParts(m_nextHandle++, 1);
        }

        ChikaEngine::Render::PipelineHandle CreateGraphicsPipeline(const ChikaEngine::Render::PipelineDesc&) override
        {
            return ChikaEngine::Render::PipelineHandle::FromParts(m_nextHandle++, 1);
        }

        ChikaEngine::Render::PipelineHandle CreateComputePipeline(const ChikaEngine::Render::ComputePipelineDesc&) override
        {
            return ChikaEngine::Render::PipelineHandle::FromParts(m_nextHandle++, 1);
        }

        void* GetMappedData(ChikaEngine::Render::BufferHandle handle) override
        {
            const auto found = m_bufferData.find(handle);
            return found == m_bufferData.end() ? nullptr : found->second.data();
        }

        void SetDebugName(ChikaEngine::Render::BufferHandle, std::string_view) override {}
        void SetDebugName(ChikaEngine::Render::TextureHandle, std::string_view) override {}
        void SetDebugName(ChikaEngine::Render::ShaderHandle, std::string_view) override {}
        void SetDebugName(ChikaEngine::Render::PipelineHandle, std::string_view) override {}
        void SetDebugName(ChikaEngine::Render::SamplerHandle, std::string_view) override {}
        void SetDebugName(ChikaEngine::Render::TextureViewHandle, std::string_view) override {}

        const ChikaEngine::Render::RenderFrameStatistics& GetFrameStatistics() const override
        {
            return m_statistics;
        }

        const std::vector<ChikaEngine::Render::RenderPassGpuTiming>& GetPassGpuTimings() const override
        {
            return m_gpuTimings;
        }

        ChikaEngine::Render::IRHICommandList* AllocateCommandList() override
        {
            return nullptr;
        }

        void Submit(ChikaEngine::Render::IRHICommandList*) override {}
        void DestroyBuffer(ChikaEngine::Render::BufferHandle) override {}
        void DestroyTexture(ChikaEngine::Render::TextureHandle) override {}
        void DestroyShader(ChikaEngine::Render::ShaderHandle) override {}
        void DestroyPipeline(ChikaEngine::Render::PipelineHandle) override {}
        void DestroySampler(ChikaEngine::Render::SamplerHandle) override {}
        void DestroyTextureView(ChikaEngine::Render::TextureViewHandle) override {}

        ChikaEngine::Render::TextureHandle GetActiveSwapchainTexture() override
        {
            return {};
        }

        void WaitIdle() override {}
        void Resize(uint32_t, uint32_t) override {}

      private:
        uint32_t m_nextHandle = 0;
        std::unordered_map<ChikaEngine::Render::BufferHandle, std::vector<uint8_t>> m_bufferData;
        ChikaEngine::Render::RenderFrameStatistics m_statistics;
        std::vector<ChikaEngine::Render::RenderPassGpuTiming> m_gpuTimings;
    };

    bool WriteText(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::trunc);
        file << text;
        return file.good();
    }

    std::string QuotePath(const std::filesystem::path& path)
    {
        return "\"" + path.generic_string() + "\"";
    }

    void TestTextureDimensionValidation()
    {
        namespace Render = ChikaEngine::Render;

        const Render::TextureDesc cube{
            .width = 64,
            .height = 64,
            .mipLevels = 1,
            .arrayLayers = 6,
            .dimension = Render::TextureDimension::TextureCube,
        };
        Check(Render::IsTextureDescValid(cube), "valid cube texture desc is accepted");

        Render::TextureDesc invalidCubeLayers = cube;
        invalidCubeLayers.arrayLayers = 5;
        Check(!Render::IsTextureDescValid(invalidCubeLayers), "cube texture requires exactly 6 layers");

        Render::TextureDesc invalidCubeSize = cube;
        invalidCubeSize.height = 32;
        Check(!Render::IsTextureDescValid(invalidCubeSize), "cube texture requires square faces");

        Render::TextureDesc invalid2D{
            .width = 64,
            .height = 64,
            .arrayLayers = 2,
            .dimension = Render::TextureDimension::Texture2D,
        };
        Check(!Render::IsTextureDescValid(invalid2D), "2D texture desc rejects multiple array layers");

        const Render::TextureViewDesc cubeView{
            .range = { .baseMipLevel = 0, .mipLevelCount = 1, .baseArrayLayer = 0, .arrayLayerCount = 6 },
            .dimension = Render::TextureDimension::TextureCube,
        };
        Check(Render::IsTextureViewRangeValid(cube, cubeView), "cube view accepts all 6 layers");

        Render::TextureViewDesc invalidCubeView = cubeView;
        invalidCubeView.range.arrayLayerCount = 5;
        Check(!Render::IsTextureViewRangeValid(cube, invalidCubeView), "cube view rejects partial layer ranges");

        Render::TextureViewDesc invalid2DView = cubeView;
        invalid2DView.dimension = Render::TextureDimension::Texture2D;
        Check(!Render::IsTextureViewRangeValid(cube, invalid2DView), "2D view cannot span 6 cube layers");
    }

    void TestTextureDescriptorLoading()
    {
        namespace Asset = ChikaEngine::Asset;

        const std::filesystem::path root = std::filesystem::temp_directory_path() / "chika_environment_resource_tests";
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root, error);
        Check(!error, "temporary descriptor directory is created");

        const std::filesystem::path skyboxRoot = std::filesystem::current_path() / "Assets" / "Textures" / "Skybox";
        const std::filesystem::path descriptor = root / "environment.texture";
        const std::string descriptorJson = std::string("{\n")
                                         + "  \"usage\": \"environment\",\n"
                                         + "  \"srgb\": false,\n"
                                         + "  \"generateMips\": true,\n"
                                         + "  \"cubeFaces\": {\n"
                                         + "    \"px\": " + QuotePath(skyboxRoot / "px.png") + ",\n"
                                         + "    \"nx\": " + QuotePath(skyboxRoot / "nx.png") + ",\n"
                                         + "    \"py\": " + QuotePath(skyboxRoot / "py.png") + ",\n"
                                         + "    \"ny\": " + QuotePath(skyboxRoot / "ny.png") + ",\n"
                                         + "    \"pz\": " + QuotePath(skyboxRoot / "pz.png") + ",\n"
                                         + "    \"nz\": " + QuotePath(skyboxRoot / "nz.png") + "\n"
                                         + "  }\n"
                                         + "}\n";
        Check(WriteText(descriptor, descriptorJson), "environment descriptor is written");

        std::unique_ptr<Asset::TextureData> texture = Asset::TextureLoader::Load(descriptor.string());
        Check(texture != nullptr, "environment descriptor loads");
        if (texture)
        {
            Check(texture->usage == Asset::TextureAssetUsage::Environment, "descriptor keeps environment usage");
            Check(texture->shape == Asset::TextureShape::TextureCube, "descriptor creates cube texture shape");
            Check(texture->arrayLayers == 6, "descriptor creates 6 cube layers");
            Check(!texture->srgb, "environment descriptor stays linear when srgb is false");
            Check(texture->generateMips, "descriptor records generateMips metadata");
            Check(texture->cubeFaces.size() == 6, "descriptor stores face source paths");
            Check(texture->pixels.size() == static_cast<size_t>(texture->width) * texture->height * 4u * 6u, "descriptor packs all cube face pixels");
        }

        const std::filesystem::path invalidDescriptor = root / "invalid.texture";
        Check(WriteText(invalidDescriptor, R"({"usage":"environment","cubeFaces":["a.png","b.png","c.png","d.png","e.png"]})"), "invalid descriptor is written");
        Check(Asset::TextureLoader::Load(invalidDescriptor.string()) == nullptr, "descriptor rejects illegal cube face count");

        const std::filesystem::path fallbackDescriptor = root / "fallback.texture";
        Check(WriteText(fallbackDescriptor, R"({"usage":"environment_irradiance","fallback":"gray_irradiance"})"), "fallback descriptor is written");
        std::unique_ptr<Asset::TextureData> fallback = Asset::TextureLoader::Load(fallbackDescriptor.string());
        Check(fallback != nullptr, "fallback descriptor creates texture data");
        if (fallback)
        {
            Check(fallback->usage == Asset::TextureAssetUsage::EnvironmentIrradiance, "fallback keeps irradiance usage");
            Check(fallback->shape == Asset::TextureShape::TextureCube, "gray irradiance fallback is a cubemap");
            Check(fallback->arrayLayers == 6, "gray irradiance fallback has 6 layers");
            Check(!fallback->srgb, "fallback environment texture is linear");
            Check(fallback->pixels.size() == 24, "fallback cubemap stores one RGBA texel per face");
        }

        Check(Asset::AssetDatabase::Classify("probe.texture") == Asset::AssetType::Texture, "asset database classifies .texture descriptors as textures");
        std::filesystem::remove_all(root, error);
    }

    void TestEnvironmentResolutionAndGraphImport()
    {
        namespace Asset = ChikaEngine::Asset;
        namespace Render = ChikaEngine::Render;
        namespace Resource = ChikaEngine::Resource;

        Asset::AssetManager assets;
        Check(assets.Initialize({
                  .assetRoot = "Assets",
                  .createRoot = false,
                  .scanAssets = true,
                  .createMissingMeta = false,
                  .importAssets = false,
                  .enableHotReload = false,
              }),
              "asset manager initializes for environment integration");

        const Asset::AssetRecord* skyboxRecord = assets.GetDatabase().FindByPath("Assets/Textures/Skybox/default-skybox.texture");
        Check(skyboxRecord != nullptr, "default skybox descriptor has a stable asset record");
        if (!skyboxRecord)
            return;

        EnvironmentTestRHI rhi;
        Resource::ResourceManager resources(rhi, assets);
        Render::EnvironmentResourceResolver resolver;
        Render::EnvironmentSettings settings;
        settings.enabled = true;
        settings.skybox = Asset::AssetReference(skyboxRecord->guid, Asset::AssetType::Texture, {}, skyboxRecord->sourcePath.generic_string());

        Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::Ready, "environment resolver produces a ready cubemap");
        const Render::EnvironmentTextureResource skybox = resolver.GetSkybox();
        Check(skybox.IsValid(), "resolved skybox satisfies the render import contract");
        Check(skybox.desc.dimension == Render::TextureDimension::TextureCube, "resolved skybox keeps cube dimension");
        Check(skybox.desc.format == Render::RHI_Format::RGBA8_UNorm, "resolved skybox keeps the uploaded texture format");

        const std::vector<Resource::TextureUploadRequest> uploadJobs = resources.GetTextureUploadJobs();
        Check(uploadJobs.size() == 1, "first environment resolution queues one texture upload");
        if (!uploadJobs.empty())
        {
            Render::RenderGraph graph(nullptr);
            Render::RenderGraphBlackboard blackboard;
            Render::ImportedTextureMap pendingUploads;
            const Resource::TextureUploadRequest& upload = uploadJobs.front();
            const Render::TextureDesc uploadDesc{
                .width = upload.width,
                .height = upload.height,
                .format = upload.format,
                .mipLevels = upload.mipLevels,
                .arrayLayers = upload.arrayLayers,
                .usage = Render::RHI_TextureUsage::Sampled,
                .dimension = upload.dimension,
            };
            const Render::RGTextureHandle uploadHandle = graph.ImportTexture("Upload.Texture.Destination.0", upload.dst, uploadDesc, Render::ResourceState::Undefined, Render::ResourceState::ShaderResource);
            pendingUploads.emplace(upload.dst, uploadHandle);

            const Render::RGTextureHandle published = Render::PublishEnvironmentSkybox(graph, blackboard, skybox, pendingUploads);
            Check(published == uploadHandle, "skybox reuses the same-frame upload destination RG handle");
            Check(blackboard.GetTexture(Render::RenderGraphSemantic::EnvironmentSkybox) == uploadHandle, "blackboard publishes Environment.Skybox");
            Check(graph.GetPhysicalTexture(published) == skybox.texture, "published skybox keeps the resolved physical texture");
        }

        Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::Ready, "cached environment resource remains ready");
        Check(resources.GetTextureUploadJobs().empty(), "cached environment resource does not upload again");

        {
            Render::RenderGraph graph(nullptr);
            Render::RenderGraphBlackboard blackboard;
            const Render::RGTextureHandle published = Render::PublishEnvironmentSkybox(graph, blackboard, resolver.GetSkybox(), {});
            Check(published.IsValid(), "ready cached skybox imports without a pending upload");
            Check(graph.GetPhysicalTexture(published) == resolver.GetSkybox().texture, "ready cached skybox import keeps its physical texture");
        }

        resources.UnloadAll();
        Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::Ready, "resolver recovers after ResourceManager invalidates cached handles");
        Check(resources.GetTextureUploadJobs().size() == 1, "stale environment handle triggers exactly one replacement upload");

        const Asset::AssetRecord* texture2DRecord = assets.GetDatabase().FindByPath("Assets/Textures/Skybox/px.png");
        Check(texture2DRecord != nullptr, "2D texture fixture has an asset record");
        if (texture2DRecord)
        {
            settings.skybox = Asset::AssetReference(texture2DRecord->guid, Asset::AssetType::Texture, {}, texture2DRecord->sourcePath.generic_string());
            Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::InvalidTextureContract, "environment resolver rejects a 2D skybox texture");
        }

        settings.enabled = false;
        Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::Disabled, "disabled environment clears the resolved resource");
        Check(!resolver.GetSkybox().IsValid(), "disabled environment publishes no cubemap");
    }

    void TestRenderEnvironmentSemantics()
    {
        namespace Render = ChikaEngine::Render;
        Check(Render::RenderGraphSemantic::EnvironmentSkybox == "Environment.Skybox", "skybox semantic name is stable");
        Check(Render::RenderGraphSemantic::EnvironmentIrradiance == "Environment.Irradiance", "irradiance semantic name is stable");
        Check(Render::RenderGraphSemantic::EnvironmentPrefiltered == "Environment.Prefiltered", "prefiltered semantic name is stable");
        Check(Render::RenderGraphSemantic::EnvironmentBRDFLut == "Environment.BRDFLut", "BRDF LUT semantic name is stable");
    }

    void TestTextureGpuContract()
    {
        namespace Asset = ChikaEngine::Asset;
        namespace Render = ChikaEngine::Render;
        namespace Resource = ChikaEngine::Resource;

        const Resource::TextureGPU gpu{
            .texture = Render::TextureHandle::FromParts(1, 1),
            .defaultView = Render::TextureViewHandle::FromParts(2, 1),
            .sampler = Render::SamplerHandle::FromParts(3, 1),
            .faceViews = {
                Render::TextureViewHandle::FromParts(4, 1),
                Render::TextureViewHandle::FromParts(5, 1),
                Render::TextureViewHandle::FromParts(6, 1),
                Render::TextureViewHandle::FromParts(7, 1),
                Render::TextureViewHandle::FromParts(8, 1),
                Render::TextureViewHandle::FromParts(9, 1),
            },
            .dimension = Render::TextureDimension::TextureCube,
            .format = Render::RHI_Format::RGBA8_UNorm,
            .usage = Asset::TextureAssetUsage::Environment,
            .width = 64,
            .height = 64,
            .mipLevels = 1,
            .arrayLayers = 6,
        };
        Check(gpu.texture.IsValid(), "texture gpu stores texture handle");
        Check(gpu.defaultView.IsValid(), "texture gpu stores default view handle");
        Check(gpu.sampler.IsValid(), "texture gpu stores sampler handle");
        Check(gpu.faceViews.size() == 6, "texture gpu stores cube face views");
        Check(gpu.dimension == Render::TextureDimension::TextureCube, "texture gpu stores dimension");
        Check(gpu.format == Render::RHI_Format::RGBA8_UNorm, "texture gpu stores upload format");
        Check(gpu.usage == Asset::TextureAssetUsage::Environment, "texture gpu stores usage");
        Check(gpu.arrayLayers == 6, "texture gpu stores layer count");
    }
} // namespace

int main()
{
    TestTextureDimensionValidation();
    TestTextureDescriptorLoading();
    TestEnvironmentResolutionAndGraphImport();
    TestRenderEnvironmentSemantics();
    TestTextureGpuContract();

    if (g_failures == 0)
        std::cout << "Environment resource contract checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
