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
#include "ChikaEngine/jobs/JobSystem.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

extern "C"
{
#include <exr.h>
}

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
        explicit EnvironmentTestRHI(bool failTextureCreation = false, uint32_t maxCubeSize = 0) : m_failTextureCreation(failTextureCreation)
        {
            m_capabilities.maxTextureCubeSize = maxCubeSize;
        }

        void Initialize(const ChikaEngine::Render::RHI_InitParams&) override {}
        void Shutdown() override {}
        void BeginFrame() override {}
        void EndFrame() override {}

        const ChikaEngine::Render::RHICapabilities& GetCapabilities() const override
        {
            return m_capabilities;
        }

        ChikaEngine::Render::BufferHandle CreateBuffer(const ChikaEngine::Render::BufferDesc& desc) override
        {
            const auto handle = ChikaEngine::Render::BufferHandle::FromParts(m_nextHandle++, 1);
            m_bufferData[handle].resize(static_cast<size_t>(desc.size));
            return handle;
        }

        ChikaEngine::Render::TextureHandle CreateTexture(const ChikaEngine::Render::TextureDesc& desc) override
        {
            if (m_failTextureCreation)
                return {};
            const auto handle = ChikaEngine::Render::TextureHandle::FromParts(m_nextHandle++, 1);
            m_textureDescs[handle] = desc;
            return handle;
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

        const std::vector<uint8_t>* GetBufferData(ChikaEngine::Render::BufferHandle handle) const
        {
            const auto found = m_bufferData.find(handle);
            return found == m_bufferData.end() ? nullptr : &found->second;
        }

        const ChikaEngine::Render::TextureDesc* GetTextureDesc(ChikaEngine::Render::TextureHandle handle) const
        {
            const auto found = m_textureDescs.find(handle);
            return found == m_textureDescs.end() ? nullptr : &found->second;
        }

      private:
        bool m_failTextureCreation = false;
        ChikaEngine::Render::RHICapabilities m_capabilities;
        uint32_t m_nextHandle = 0;
        std::unordered_map<ChikaEngine::Render::BufferHandle, std::vector<uint8_t>> m_bufferData;
        std::unordered_map<ChikaEngine::Render::TextureHandle, ChikaEngine::Render::TextureDesc> m_textureDescs;
        ChikaEngine::Render::RenderFrameStatistics m_statistics;
        std::vector<ChikaEngine::Render::RenderPassGpuTiming> m_gpuTimings;
    };

    bool WriteText(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::trunc);
        file << text;
        return file.good();
    }

    std::array<uint8_t, 4> EncodeRGBE(const std::array<float, 3>& rgb)
    {
        const float maximum = std::max({ rgb[0], rgb[1], rgb[2] });
        if (maximum < 1.0e-32f)
            return { 0, 0, 0, 0 };
        int exponent = 0;
        const float mantissa = std::frexp(maximum, &exponent);
        const float scale = mantissa * 256.0f / maximum;
        return {
            static_cast<uint8_t>(rgb[0] * scale),
            static_cast<uint8_t>(rgb[1] * scale),
            static_cast<uint8_t>(rgb[2] * scale),
            static_cast<uint8_t>(exponent + 128),
        };
    }

    bool WriteRadianceHDR(const std::filesystem::path& path, uint32_t width, uint32_t height, const std::array<float, 3>& rgb)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y " << height << " +X " << width << '\n';
        const std::array<uint8_t, 4> rgbe = EncodeRGBE(rgb);
        for (uint64_t pixel = 0; pixel < static_cast<uint64_t>(width) * height; ++pixel)
            file.write(reinterpret_cast<const char*>(rgbe.data()), static_cast<std::streamsize>(rgbe.size()));
        return file.good();
    }

    bool WriteRadianceHDR(const std::filesystem::path& path, const std::array<float, 3>& rgb)
    {
        return WriteRadianceHDR(path, 1, 1, rgb);
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
        const std::string descriptorJson = std::string("{\n") + "  \"usage\": \"environment\",\n" + "  \"srgb\": false,\n" + "  \"generateMips\": false,\n" + "  \"cubeFaces\": {\n" + "    \"px\": " + QuotePath(skyboxRoot / "px.png") + ",\n" + "    \"nx\": " + QuotePath(skyboxRoot / "nx.png") + ",\n" + "    \"py\": " + QuotePath(skyboxRoot / "py.png") + ",\n" + "    \"ny\": " + QuotePath(skyboxRoot / "ny.png") + ",\n" + "    \"pz\": " + QuotePath(skyboxRoot / "pz.png") + ",\n" +
                                           "    \"nz\": " + QuotePath(skyboxRoot / "nz.png") + "\n" + "  }\n" + "}\n";
        Check(WriteText(descriptor, descriptorJson), "environment descriptor is written");

        std::unique_ptr<Asset::TextureData> texture = Asset::TextureLoader::Load(descriptor.string());
        Check(texture != nullptr, "environment descriptor loads");
        if (texture)
        {
            Check(texture->usage == Asset::TextureAssetUsage::Environment, "descriptor keeps environment usage");
            Check(texture->shape == Asset::TextureShape::TextureCube, "descriptor creates cube texture shape");
            Check(texture->arrayLayers == 6, "descriptor creates 6 cube layers");
            Check(!texture->srgb, "environment descriptor stays linear when srgb is false");
            Check(!texture->generateMips, "descriptor disables mip generation until a real generator exists");
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
        Check(skybox.desc.format == Render::RHI_Format::RGBA8_SRGB, "default LDR skybox is sampled through the sRGB format");

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
            settings.useFallback = false;
            settings.skybox = Asset::AssetReference(texture2DRecord->guid, Asset::AssetType::Texture, {}, texture2DRecord->sourcePath.generic_string());
            Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::InvalidTextureContract, "environment resolver rejects a 2D skybox texture");

            settings.useFallback = true;
            Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::ReadyFallback, "invalid Skybox uses the packaged fallback Cubemap when enabled");
            Check(resolver.IsUsingFallback() && resolver.GetSkybox().IsValid(), "fallback resolution publishes only a valid Cubemap");
        }

        settings.skybox = {};
        Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::ReadyFallback, "missing Skybox reference uses the packaged fallback Cubemap");

        settings.useFallback = false;
        Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::MissingReference, "missing Skybox without fallback remains unavailable");
        Check(!resolver.GetSkybox().IsValid(), "unavailable Skybox never publishes an invalid descriptor");

        settings.enabled = false;
        Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::Disabled, "disabled environment clears the resolved resource");
        Check(!resolver.GetSkybox().IsValid(), "disabled environment publishes no cubemap");
    }

    void TestEnvironmentResolutionDoesNotBlockOnAssetLoad()
    {
        namespace Asset = ChikaEngine::Asset;
        namespace Jobs = ChikaEngine::Jobs;
        namespace Render = ChikaEngine::Render;
        namespace Resource = ChikaEngine::Resource;

        Jobs::JobSystem jobs;
        if (!jobs.Initialize({ .workerCount = 1, .jobCapacity = 64 }))
        {
            Check(false, "job system initializes for asynchronous environment loading");
            return;
        }

        std::mutex gateMutex;
        std::condition_variable gateCondition;
        bool blockerStarted = false;
        bool releaseBlocker = false;
        const Jobs::JobHandle blocker = jobs.Schedule("Test.EnvironmentLoadGate",
                                                      [&]
                                                      {
                                                          std::unique_lock lock(gateMutex);
                                                          blockerStarted = true;
                                                          gateCondition.notify_all();
                                                          gateCondition.wait(lock, [&] { return releaseBlocker; });
                                                      });
        if (!blocker.IsValid())
        {
            Check(false, "environment loading gate job is scheduled");
            jobs.Shutdown(Jobs::JobShutdownPolicy::CancelPending);
            return;
        }
        {
            std::unique_lock lock(gateMutex);
            gateCondition.wait(lock, [&] { return blockerStarted; });
        }

        Asset::AssetManager assets;
        Check(assets.Initialize({
                  .assetRoot = "Assets",
                  .createRoot = false,
                  .scanAssets = true,
                  .createMissingMeta = false,
                  .importAssets = false,
                  .enableHotReload = false,
                  .jobSystem = &jobs,
              }),
              "asset manager initializes with a job system for environment loading");

        const Asset::AssetRecord* skyboxRecord = assets.GetDatabase().FindByPath("Assets/Textures/Skybox/default-skybox.texture");
        Check(skyboxRecord != nullptr, "asynchronous environment test resolves the default descriptor");
        if (skyboxRecord)
        {
            EnvironmentTestRHI rhi;
            Resource::ResourceManager resources(rhi, assets);
            Render::EnvironmentResourceResolver resolver;
            Render::EnvironmentSettings settings;
            settings.enabled = true;
            settings.useFallback = false;
            settings.skybox = Asset::AssetReference(skyboxRecord->guid, Asset::AssetType::Texture, {}, skyboxRecord->sourcePath.generic_string());

            Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::Loading, "environment resolution returns immediately while the asset worker is occupied");
            Check(!resolver.GetSkybox().IsValid(), "loading environment does not publish an incomplete GPU texture");

            {
                std::lock_guard lock(gateMutex);
                releaseBlocker = true;
            }
            gateCondition.notify_all();
            jobs.Wait(blocker);
            jobs.Release(blocker);

            Render::EnvironmentResourceStatus status = Render::EnvironmentResourceStatus::Loading;
            for (uint32_t attempt = 0; attempt < 200 && status == Render::EnvironmentResourceStatus::Loading; ++attempt)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                status = resolver.Update(settings, assets, resources);
            }
            Check(status == Render::EnvironmentResourceStatus::Ready, "environment becomes ready after the asynchronous asset job completes");
            Check(resolver.GetSkybox().IsValid(), "completed asynchronous load publishes a valid Cubemap");
        }
        else
        {
            {
                std::lock_guard lock(gateMutex);
                releaseBlocker = true;
            }
            gateCondition.notify_all();
            jobs.Wait(blocker);
            jobs.Release(blocker);
        }

        assets.Shutdown();
        jobs.Shutdown(Jobs::JobShutdownPolicy::Drain);
    }

    void TestFloatingPointUploadContract()
    {
        namespace Asset = ChikaEngine::Asset;
        namespace Render = ChikaEngine::Render;
        namespace Resource = ChikaEngine::Resource;

        const std::filesystem::path root = std::filesystem::temp_directory_path() / "chika_float_texture_upload_tests";
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root, error);
        Check(!error, "floating-point upload fixture directory is created");

        const std::filesystem::path hdr = root / "probe.hdr";
        const std::filesystem::path halfDescriptor = root / "probe-half.texture";
        const std::filesystem::path floatDescriptor = root / "probe-float.texture";
        Check(WriteRadianceHDR(hdr, { 8.0f, 2.0f, 0.5f }), "floating-point upload HDR fixture is written");
        Check(WriteText(halfDescriptor, R"({"source":"probe.hdr","format":"rgba16f","srgb":false,"generateMips":false})"), "RGBA16F upload descriptor is written");
        Check(WriteText(floatDescriptor, R"({"source":"probe.hdr","format":"rgba32f","srgb":false,"generateMips":false})"), "RGBA32F upload descriptor is written");

        Asset::AssetManager assets;
        Check(assets.Initialize({
                  .assetRoot = root,
                  .createRoot = false,
                  .scanAssets = true,
                  .createMissingMeta = true,
                  .importAssets = false,
                  .enableHotReload = false,
              }),
              "asset manager initializes for floating-point upload test");
        EnvironmentTestRHI rhi;
        Resource::ResourceManager resources(rhi, assets);

        const auto verifyUpload = [&](const std::filesystem::path& descriptor, Render::RHI_Format expectedFormat, uint64_t expectedBytes)
        {
            const Asset::TextureHandle asset = assets.LoadTexture(descriptor.string());
            Check(asset.IsValid(), "floating-point descriptor loads through AssetManager");
            if (!asset.IsValid())
                return;

            const Resource::TextureHandle resource = resources.UploadTexture(asset);
            Check(resource.IsValid(), "floating-point texture uploads through ResourceManager");
            Check(resources.GetTextureUploadStatus(asset) == Resource::TextureUploadStatus::Ready, "floating-point upload status is Ready");
            const Resource::TextureGPU* gpu = resources.TryGetTexture(resource);
            Check(gpu && gpu->format == expectedFormat, "TextureGPU format matches the float payload");

            const std::vector<Resource::TextureUploadRequest> jobs = resources.GetTextureUploadJobs();
            Check(jobs.size() == 1, "floating-point texture queues one upload request");
            if (jobs.empty())
                return;
            const Resource::TextureUploadRequest& upload = jobs.front();
            Check(upload.format == expectedFormat, "upload request format matches TextureGPU");
            Check(upload.rowBytes == expectedBytes && upload.layerBytes == expectedBytes && upload.size == expectedBytes, "upload request carries an exact tight byte layout");
            Check(upload.size == static_cast<uint64_t>(upload.width) * upload.height * upload.arrayLayers * Render::RHIFormatBytesPerTexel(upload.format), "staging bytes equal extent times format bytes-per-texel");
            const Render::TextureDesc* desc = rhi.GetTextureDesc(upload.dst);
            Check(desc && desc->format == upload.format, "RHI TextureDesc format matches the upload request");

            const std::vector<uint8_t>* staging = rhi.GetBufferData(upload.staging);
            Check(staging && staging->size() == expectedBytes, "staging buffer stores the complete float payload");
            if (!staging)
                return;
            float red = 0.0f;
            if (expectedFormat == Render::RHI_Format::RGBA16_Float)
            {
                uint16_t half = 0;
                std::memcpy(&half, staging->data(), sizeof(half));
                exr_half_to_float(&half, &red, 1);
            }
            else
                std::memcpy(&red, staging->data(), sizeof(red));
            Check(red > 1.0f, "radiance above 1.0 survives into the staging payload");
        };

        verifyUpload(halfDescriptor, Render::RHI_Format::RGBA16_Float, 8);
        verifyUpload(floatDescriptor, Render::RHI_Format::RGBA32_Float, 16);

        const std::filesystem::path panorama = root / "panorama.hdr";
        const std::filesystem::path panoramaDescriptor = root / "panorama.texture";
        Check(WriteRadianceHDR(panorama, 8, 4, { 6.0f, 1.0f, 0.25f }), "equirectangular upload HDR fixture is written");
        Check(WriteText(panoramaDescriptor, R"({"usage":"environment","source":"panorama.hdr","projection":"equirectangular","outputFaceSize":4,"format":"rgba16f","srgb":false})"), "equirectangular upload descriptor is written");
        const Asset::TextureHandle panoramaAsset = assets.LoadTexture(panoramaDescriptor.string());
        Check(panoramaAsset.IsValid(), "equirectangular descriptor loads through AssetManager");
        if (panoramaAsset.IsValid())
        {
            const Resource::TextureHandle panoramaResource = resources.UploadTexture(panoramaAsset);
            const Resource::TextureGPU* gpu = resources.TryGetTexture(panoramaResource);
            Check(gpu && gpu->dimension == Render::TextureDimension::TextureCube && gpu->format == Render::RHI_Format::RGBA16_Float && gpu->arrayLayers == 6 && gpu->width == 4, "projected environment uploads as a six-layer float Cubemap");
            const std::vector<Resource::TextureUploadRequest> jobs = resources.GetTextureUploadJobs();
            Check(jobs.size() == 1 && jobs.front().size == 4u * 4u * 6u * 8u && jobs.front().layerBytes == 4u * 4u * 8u, "projected Cubemap upload covers every face with exact float layout");
            if (!jobs.empty())
            {
                const std::vector<uint8_t>* staging = rhi.GetBufferData(jobs.front().staging);
                uint16_t half = 0;
                float red = 0.0f;
                if (staging && staging->size() >= sizeof(half))
                {
                    std::memcpy(&half, staging->data(), sizeof(half));
                    exr_half_to_float(&half, &red, 1);
                }
                Check(red > 1.0f, "projected HDR radiance survives into the Cubemap staging payload");
            }

            EnvironmentTestRHI limitedRhi(false, 2);
            Resource::ResourceManager limitedResources(limitedRhi, assets);
            Check(!limitedResources.UploadTexture(panoramaAsset).IsValid() && limitedResources.GetTextureUploadStatus(panoramaAsset) == Resource::TextureUploadStatus::DimensionLimitExceeded, "projected face size is checked against the active RHI Cubemap limit");
        }
        std::filesystem::remove_all(root, error);
    }

    void TestRenderEnvironmentSemantics()
    {
        namespace Render = ChikaEngine::Render;
        Check(Render::RenderGraphSemantic::EnvironmentSkybox == "Environment.Skybox", "skybox semantic name is stable");
        Check(Render::RenderGraphSemantic::EnvironmentIrradiance == "Environment.Irradiance", "irradiance semantic name is stable");
        Check(Render::RenderGraphSemantic::EnvironmentPrefiltered == "Environment.Prefiltered", "prefiltered semantic name is stable");
        Check(Render::RenderGraphSemantic::EnvironmentBRDFLut == "Environment.BRDFLut", "BRDF LUT semantic name is stable");
    }

    void TestUnavailableFallbackNeverPublishes()
    {
        namespace Asset = ChikaEngine::Asset;
        namespace Render = ChikaEngine::Render;
        namespace Resource = ChikaEngine::Resource;

        const std::filesystem::path root = std::filesystem::temp_directory_path() / "chika_missing_environment_fallback";
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root, error);

        Asset::AssetManager assets;
        Check(assets.Initialize({
                  .assetRoot = root,
                  .createRoot = false,
                  .scanAssets = true,
                  .createMissingMeta = false,
                  .importAssets = false,
                  .enableHotReload = false,
              }),
              "empty asset manager initializes for missing fallback test");
        EnvironmentTestRHI rhi;
        Resource::ResourceManager resources(rhi, assets);
        Render::EnvironmentResourceResolver resolver;
        Render::EnvironmentSettings settings;
        settings.enabled = true;
        settings.useFallback = true;

        Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::FallbackUnavailable, "missing packaged fallback has an explicit status");
        Check(!resolver.GetSkybox().IsValid(), "missing packaged fallback does not expose an invalid descriptor");
        std::filesystem::remove_all(root, error);
    }

    void TestEnvironmentFailureStatuses()
    {
        namespace Asset = ChikaEngine::Asset;
        namespace Render = ChikaEngine::Render;
        namespace Resource = ChikaEngine::Resource;

        const std::filesystem::path root = std::filesystem::temp_directory_path() / "chika_environment_failure_status_tests";
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root, error);
        const std::filesystem::path invalidHdr = root / "invalid.hdr";
        const std::filesystem::path invalidDescriptor = root / "invalid.texture";
        Check(WriteText(invalidHdr, "not a radiance image"), "invalid HDR fixture is written");
        Check(WriteText(invalidDescriptor, R"({"source":"invalid.hdr","format":"rgba16f","srgb":false})"), "invalid HDR descriptor is written");

        Asset::AssetManager invalidAssets;
        Check(invalidAssets.Initialize({
                  .assetRoot = root,
                  .createRoot = false,
                  .scanAssets = true,
                  .createMissingMeta = true,
                  .importAssets = false,
                  .enableHotReload = false,
              }),
              "asset manager initializes for decode status test");
        const Asset::AssetRecord* invalidRecord = invalidAssets.GetDatabase().FindByPath(invalidDescriptor);
        Check(invalidRecord != nullptr, "invalid HDR descriptor has an asset record");
        if (invalidRecord)
        {
            EnvironmentTestRHI rhi;
            Resource::ResourceManager resources(rhi, invalidAssets);
            Render::EnvironmentResourceResolver resolver;
            Render::EnvironmentSettings settings;
            settings.enabled = true;
            settings.useFallback = false;
            settings.skybox = Asset::AssetReference(invalidRecord->guid, Asset::AssetType::Texture, {}, invalidRecord->sourcePath.generic_string());
            Check(resolver.Update(settings, invalidAssets, resources) == Render::EnvironmentResourceStatus::TextureDecodeFailed, "environment status distinguishes texture decode failure");
        }

        Asset::AssetManager assets;
        Check(assets.Initialize({
                  .assetRoot = "Assets",
                  .createRoot = false,
                  .scanAssets = true,
                  .createMissingMeta = false,
                  .importAssets = false,
                  .enableHotReload = false,
              }),
              "asset manager initializes for GPU upload failure status test");
        const Asset::AssetRecord* skyboxRecord = assets.GetDatabase().FindByPath("Assets/Textures/Skybox/default-skybox.texture");
        Check(skyboxRecord != nullptr, "default skybox record exists for GPU upload failure test");
        if (skyboxRecord)
        {
            EnvironmentTestRHI failingRhi(true);
            Resource::ResourceManager resources(failingRhi, assets);
            Render::EnvironmentResourceResolver resolver;
            Render::EnvironmentSettings settings;
            settings.enabled = true;
            settings.useFallback = false;
            settings.skybox = Asset::AssetReference(skyboxRecord->guid, Asset::AssetType::Texture, {}, skyboxRecord->sourcePath.generic_string());
            Check(resolver.Update(settings, assets, resources) == Render::EnvironmentResourceStatus::ResourceUploadFailed, "environment status distinguishes GPU upload failure");
        }
        std::filesystem::remove_all(root, error);
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
    TestFloatingPointUploadContract();
    TestEnvironmentResolutionAndGraphImport();
    TestEnvironmentResolutionDoesNotBlockOnAssetLoad();
    TestRenderEnvironmentSemantics();
    TestUnavailableFallbackNeverPublishes();
    TestEnvironmentFailureStatuses();
    TestTextureGpuContract();

    if (g_failures == 0)
        std::cout << "Environment resource contract checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
