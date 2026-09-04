#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/IRHICommandList.hpp"
#include "ChikaEngine/IRHIDevice.hpp"
#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/ResourceManager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{
    using namespace ChikaEngine;

    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    template <size_t N> bool ValuesEqual(const std::vector<float>& values, const std::array<float, N>& expected)
    {
        if (values.size() != expected.size())
            return false;
        for (size_t index = 0; index < expected.size(); ++index)
        {
            if (std::fabs(values[index] - expected[index]) > 0.0001f)
                return false;
        }
        return true;
    }

    const Resource::MaterialParameterInfo* FindParameter(const std::vector<Resource::MaterialParameterInfo>& parameters, std::string_view name)
    {
        const auto found = std::ranges::find(parameters, name, &Resource::MaterialParameterInfo::name);
        return found == parameters.end() ? nullptr : &*found;
    }

    template <size_t N> bool BytesContainValue(const std::vector<uint8_t>& bytes, const Resource::MaterialParameterRuntime& runtime, const std::array<float, N>& expected)
    {
        if (runtime.offset + expected.size() * sizeof(float) > bytes.size())
            return false;

        std::array<float, N> actual{};
        std::memcpy(actual.data(), bytes.data() + runtime.offset, actual.size() * sizeof(float));
        for (size_t index = 0; index < expected.size(); ++index)
        {
            if (std::fabs(actual[index] - expected[index]) > 0.0001f)
                return false;
        }
        return true;
    }

    template <size_t N> bool MappedBufferContainsValue(void* mapped, const Resource::MaterialParameterRuntime& runtime, const std::array<float, N>& expected)
    {
        if (!mapped)
            return false;

        std::array<float, N> actual{};
        const auto* bytes = static_cast<const uint8_t*>(mapped);
        std::memcpy(actual.data(), bytes + runtime.offset, actual.size() * sizeof(float));
        for (size_t index = 0; index < expected.size(); ++index)
        {
            if (std::fabs(actual[index] - expected[index]) > 0.0001f)
                return false;
        }
        return true;
    }

    class MinimalCommandList final : public Render::IRHICommandList
    {
      public:
        void Begin() override {}
        void End() override {}
        void SetDebugName(std::string_view) override {}
        void BeginDebugLabel(std::string_view, const float[4]) override {}
        void EndDebugLabel() override {}
        void BeginTimestampScope(std::string_view) override {}
        void EndTimestampScope() override {}
        void BeginRendering(const std::vector<Render::RenderingAttachment>&, const Render::RenderingAttachment*) override {}
        void EndRendering() override {}
        void InsertTextureBarrier(Render::TextureHandle, Render::ResourceState, Render::ResourceState, const Render::TextureSubresourceRange&) override {}
        void InsertBufferBarrier(Render::BufferHandle, Render::ResourceState, Render::ResourceState, const Render::BufferRange&) override {}
        void BindPipeline(Render::PipelineHandle) override {}
        void BindResources(const Render::ResourceBindingGroup&) override {}
        void BindVertexBuffer(Render::BufferHandle, uint64_t) override {}
        void BindIndexBuffer(Render::BufferHandle, uint64_t, bool) override {}
        void PushConstants(std::string_view, const void*, uint32_t) override {}
        void CopyBuffer(Render::BufferHandle, Render::BufferHandle, uint64_t) override {}
        void CopyBufferToTexture(Render::BufferHandle, Render::TextureHandle, uint32_t, uint32_t, uint32_t) override {}
        void Draw(uint32_t, uint32_t, uint32_t, uint32_t) override {}
        void DrawIndexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {}
        void DrawIndirect(Render::BufferHandle, uint64_t, uint32_t, uint32_t) override {}
        void DrawIndexedIndirect(Render::BufferHandle, uint64_t, uint32_t, uint32_t) override {}
        void Dispatch(uint32_t, uint32_t, uint32_t) override {}
    };

    class MaterialMockDevice final : public Render::IRHIDevice
    {
      public:
        void Initialize(const Render::RHI_InitParams&) override {}
        void Shutdown() override {}
        void BeginFrame() override {}
        void EndFrame() override {}

        Render::BufferHandle CreateBuffer(const Render::BufferDesc& desc) override
        {
            Render::BufferHandle handle = Render::BufferHandle::FromParts(nextHandle++, 1);
            mappedBuffers[handle.raw_value].resize(static_cast<size_t>(std::max<uint64_t>(desc.size, 1)));
            return handle;
        }
        Render::TextureHandle CreateTexture(const Render::TextureDesc&) override
        {
            return Render::TextureHandle::FromParts(nextHandle++, 1);
        }
        Render::SamplerHandle CreateSampler(const Render::SamplerDesc&) override
        {
            return Render::SamplerHandle::FromParts(nextHandle++, 1);
        }
        Render::TextureViewHandle CreateTextureView(const Render::TextureViewDesc&) override
        {
            return Render::TextureViewHandle::FromParts(nextHandle++, 1);
        }
        Render::ShaderHandle CreateShader(const Render::ShaderDesc&) override
        {
            return Render::ShaderHandle::FromParts(nextHandle++, 1);
        }
        Render::PipelineHandle CreateGraphicsPipeline(const Render::PipelineDesc&) override
        {
            return Render::PipelineHandle::FromParts(nextHandle++, 1);
        }
        Render::PipelineHandle CreateComputePipeline(const Render::ComputePipelineDesc&) override
        {
            return Render::PipelineHandle::FromParts(nextHandle++, 1);
        }

        void* GetMappedData(Render::BufferHandle handle) override
        {
            const auto found = mappedBuffers.find(handle.raw_value);
            return found == mappedBuffers.end() ? nullptr : found->second.data();
        }

        void SetDebugName(Render::BufferHandle, std::string_view) override {}
        void SetDebugName(Render::TextureHandle, std::string_view) override {}
        void SetDebugName(Render::ShaderHandle, std::string_view) override {}
        void SetDebugName(Render::PipelineHandle, std::string_view) override {}
        void SetDebugName(Render::SamplerHandle, std::string_view) override {}
        void SetDebugName(Render::TextureViewHandle, std::string_view) override {}

        const Render::RenderFrameStatistics& GetFrameStatistics() const override
        {
            return statistics;
        }
        const std::vector<Render::RenderPassGpuTiming>& GetPassGpuTimings() const override
        {
            return timings;
        }

        Render::IRHICommandList* AllocateCommandList() override
        {
            return new MinimalCommandList();
        }
        void Submit(Render::IRHICommandList* commandList) override
        {
            delete commandList;
        }

        void DestroyBuffer(Render::BufferHandle handle) override
        {
            mappedBuffers.erase(handle.raw_value);
        }
        void DestroyTexture(Render::TextureHandle) override {}
        void DestroyShader(Render::ShaderHandle) override {}
        void DestroyPipeline(Render::PipelineHandle) override {}
        void DestroySampler(Render::SamplerHandle) override {}
        void DestroyTextureView(Render::TextureViewHandle) override {}
        Render::TextureHandle GetActiveSwapchainTexture() override
        {
            return {};
        }
        void WaitIdle() override {}
        void Resize(uint32_t, uint32_t) override {}

        uint32_t nextHandle = 1;
        Render::RenderFrameStatistics statistics;
        std::vector<Render::RenderPassGpuTiming> timings;
        std::unordered_map<uint32_t, std::vector<uint8_t>> mappedBuffers;
    };

    class CurrentPathGuard
    {
      public:
        explicit CurrentPathGuard(std::filesystem::path nextPath) : m_previous(std::filesystem::current_path())
        {
            std::filesystem::current_path(std::move(nextPath));
        }

        ~CurrentPathGuard()
        {
            std::error_code error;
            std::filesystem::current_path(m_previous, error);
        }

      private:
        std::filesystem::path m_previous;
    };

    bool CopyRequiredFile(const std::filesystem::path& projectRoot, const std::filesystem::path& sandboxRoot, const std::filesystem::path& relativePath)
    {
        std::error_code error;
        const std::filesystem::path destination = sandboxRoot / relativePath;
        std::filesystem::create_directories(destination.parent_path(), error);
        if (error)
        {
            std::cerr << "FAILED: create directory for " << destination << ": " << error.message() << '\n';
            return false;
        }

        std::filesystem::copy_file(projectRoot / relativePath, destination, std::filesystem::copy_options::overwrite_existing, error);
        if (error)
        {
            std::cerr << "FAILED: copy " << relativePath << ": " << error.message() << '\n';
            return false;
        }
        return true;
    }

    bool WriteTextFile(const std::filesystem::path& path, std::string_view text)
    {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            return false;

        std::ofstream file(path, std::ios::trunc);
        file << text;
        return file.good();
    }

    bool PrepareAssetSandbox(const std::filesystem::path& projectRoot, const std::filesystem::path& sandboxRoot)
    {
        std::error_code error;
        std::filesystem::remove_all(sandboxRoot, error);
        std::filesystem::create_directories(sandboxRoot / "Assets", error);
        if (error)
        {
            std::cerr << "FAILED: create asset sandbox: " << error.message() << '\n';
            return false;
        }

        const std::array<std::filesystem::path, 16> files{
            "Assets/Materials/base.template.json",
            "Assets/Materials/base.template.json.meta",
            "Assets/Meshes/Box.gltf",
            "Assets/Meshes/Box.gltf.meta",
            "Assets/Meshes/Box0.bin",
            "Assets/Meshes/Box0.bin.meta",
            "Assets/Shaders/test.vert",
            "Assets/Shaders/test.vert.meta",
            "Assets/Shaders/test.vert.spv",
            "Assets/Shaders/test.vert.spv.reflection.json",
            "Assets/Shaders/test.frag",
            "Assets/Shaders/test.frag.meta",
            "Assets/Shaders/test.frag.spv",
            "Assets/Shaders/test.frag.spv.reflection.json",
            "Assets/Shaders/gbuffer.frag",
            "Assets/Shaders/gbuffer.frag.meta",
        };
        for (const std::filesystem::path& file : files)
        {
            if (!CopyRequiredFile(projectRoot, sandboxRoot, file))
                return false;
        }

        if (!CopyRequiredFile(projectRoot, sandboxRoot, "Assets/Shaders/gbuffer.frag.spv"))
            return false;
        if (!CopyRequiredFile(projectRoot, sandboxRoot, "Assets/Shaders/gbuffer.frag.spv.reflection.json"))
            return false;

        constexpr std::string_view materialJson = R"({
  "name": "MaterialParameterTest",
  "shader": {
    "template": {
      "guid": "ed8a6e5e71669e71740431190b98c4ad",
      "path": "Assets/Materials/base.template.json"
    }
  },
  "parameters": {
    "BaseColor": [0.8, 0.7, 0.6, 1.0],
    "Metallic": 0.2,
    "Roughness": 0.35
  }
}
)";
        constexpr std::string_view materialMeta = R"({
  "guid": "00000000000000000000000000007101",
  "imported": "Materials/material_parameter_test.json",
  "importer": "passthrough",
  "type": "material",
  "version": 1
}
)";
        return WriteTextFile(sandboxRoot / "Assets/Materials/material_parameter_test.json", materialJson) && WriteTextFile(sandboxRoot / "Assets/Materials/material_parameter_test.json.meta", materialMeta);
    }
} // namespace

int main()
{
    const std::filesystem::path projectRoot = std::filesystem::current_path();
    const std::filesystem::path sandboxRoot = std::filesystem::temp_directory_path() / "ChikaMaterialParameterTests";
    if (!PrepareAssetSandbox(projectRoot, sandboxRoot))
        return 1;

    {
        CurrentPathGuard pathGuard(sandboxRoot);

        Asset::AssetManager assets;
        Check(assets.Initialize({
                  .assetRoot = "Assets",
                  .createRoot = false,
                  .scanAssets = true,
                  .createMissingMeta = false,
                  .importAssets = false,
                  .enableHotReload = false,
              }),
              "AssetManager should initialize from material parameter sandbox");

        const Asset::MaterialHandle materialAsset = assets.LoadMaterial("Assets/Materials/material_parameter_test.json");
        const Asset::MaterialHandle materialAssetAgain = assets.LoadMaterial("Assets/Materials/material_parameter_test.json");
        const Asset::MeshHandle meshAsset = assets.LoadMesh("Assets/Meshes/Box.gltf");
        const Asset::MeshHandle meshAssetAgain = assets.LoadMesh("Assets/Meshes/Box.gltf");
        Check(materialAsset.IsValid(), "test material asset should load");
        Check(materialAsset == materialAssetAgain, "AssetManager should reuse material handles for the same source path");
        Check(meshAsset.IsValid(), "test mesh asset should load");
        Check(meshAsset == meshAssetAgain, "AssetManager should reuse mesh handles for the same source path");

        MaterialMockDevice rhi;
        {
            Resource::ResourceManager resources(rhi, assets);

            const Resource::MaterialHandle material = resources.UploadMaterial(materialAsset);
            const Resource::MaterialHandle materialAgain = resources.UploadMaterial(materialAssetAgain);
            const Resource::MeshHandle mesh = resources.UploadMesh(meshAsset);
            const Resource::MeshHandle meshAgain = resources.UploadMesh(meshAssetAgain);
            Check(material.IsValid(), "material should upload");
            Check(material == materialAgain, "ResourceManager should reuse material handles for the same asset handle");
            Check(mesh.IsValid(), "mesh should upload");
            Check(mesh == meshAgain, "ResourceManager should reuse mesh handles for the same asset handle");

            const std::vector<Resource::MaterialParameterInfo> parameters = resources.GetMaterialParameters(material);
            Check(parameters.size() == 6, "material should expose all template parameters");

            const auto* baseColor = FindParameter(parameters, "BaseColor");
            const auto* metallic = FindParameter(parameters, "Metallic");
            const auto* roughness = FindParameter(parameters, "Roughness");
            Check(baseColor && baseColor->type == Resource::MaterialParameterType::Vec4 && baseColor->componentCount == 4, "BaseColor should be vec4");
            Check(metallic && metallic->type == Resource::MaterialParameterType::Float && metallic->componentCount == 1, "Metallic should be float");
            Check(roughness && roughness->type == Resource::MaterialParameterType::Float && roughness->componentCount == 1, "Roughness should be float");
            Check(baseColor && ValuesEqual(baseColor->value, std::array{ 0.8f, 0.7f, 0.6f, 1.0f }), "BaseColor should use material override");
            Check(metallic && ValuesEqual(metallic->value, std::array{ 0.2f }), "Metallic should use material override");
            Check(roughness && ValuesEqual(roughness->value, std::array{ 0.35f }), "Roughness should use material override");

            const Resource::MaterialParameterRuntime* baseRuntime = resources.FindMaterialParameter(material, "BaseColor");
            Check(baseRuntime != nullptr, "BaseColor runtime metadata should exist");
            if (baseRuntime)
            {
                const Resource::MaterialGPU& gpu = resources.GetMaterial(material);
                Check(gpu.parameterBufferSize == 48, "material UBO size should come from reflection");
                Check(gpu.parameterData.size() == 48, "CPU shadow material buffer should keep reflected byte size");
                Check(BytesContainValue(gpu.parameterData, *baseRuntime, std::array{ 0.8f, 0.7f, 0.6f, 1.0f }), "CPU shadow data should contain initial BaseColor");
                Check(MappedBufferContainsValue(rhi.GetMappedData(gpu.uboBuffer), *baseRuntime, std::array{ 0.8f, 0.7f, 0.6f, 1.0f }), "mapped UBO should contain initial BaseColor");
            }

            const Resource::MaterialHandle materialInstance = resources.CloneMaterial(material);
            Check(materialInstance.IsValid() && materialInstance != material, "runtime material instance should clone shared material");
            const Resource::MaterialParameterRuntime* instanceBaseRuntime = resources.FindMaterialParameter(materialInstance, "BaseColor");
            if (baseRuntime && instanceBaseRuntime)
            {
                const Resource::MaterialGPU& sharedGpu = resources.GetMaterial(material);
                const Resource::MaterialGPU& instanceGpu = resources.GetMaterial(materialInstance);
                Check(instanceGpu.uboBuffer != sharedGpu.uboBuffer, "runtime material instance should own a separate parameter UBO");

                const std::array instanceBaseColor{ 0.05f, 0.15f, 0.25f, 1.0f };
                Check(resources.UpdateMaterialParameter(materialInstance, "BaseColor", instanceBaseColor), "runtime material instance update should succeed");
                Check(BytesContainValue(resources.GetMaterial(materialInstance).parameterData, *instanceBaseRuntime, instanceBaseColor), "runtime material instance CPU data should update");
                Check(MappedBufferContainsValue(rhi.GetMappedData(resources.GetMaterial(materialInstance).uboBuffer), *instanceBaseRuntime, instanceBaseColor), "runtime material instance mapped UBO should update");
                Check(BytesContainValue(resources.GetMaterial(material).parameterData, *baseRuntime, std::array{ 0.8f, 0.7f, 0.6f, 1.0f }), "runtime material instance update should not change shared CPU data");
                Check(MappedBufferContainsValue(rhi.GetMappedData(resources.GetMaterial(material).uboBuffer), *baseRuntime, std::array{ 0.8f, 0.7f, 0.6f, 1.0f }), "runtime material instance update should not change shared mapped UBO");
            }

            const std::array updatedBaseColor{ 0.25f, 0.5f, 0.75f, 1.0f };
            Check(resources.UpdateMaterialParameter(material, "BaseColor", updatedBaseColor), "BaseColor vector update should succeed");
            const Resource::MaterialParameterRuntime* updatedBaseRuntime = resources.FindMaterialParameter(material, "BaseColor");
            const Resource::MaterialGPU& updatedGpu = resources.GetMaterial(material);
            Check(updatedBaseRuntime && BytesContainValue(updatedGpu.parameterData, *updatedBaseRuntime, updatedBaseColor), "CPU shadow data should update BaseColor");
            Check(updatedBaseRuntime && MappedBufferContainsValue(rhi.GetMappedData(updatedGpu.uboBuffer), *updatedBaseRuntime, updatedBaseColor), "mapped UBO should update BaseColor");

            const std::array wrongComponentCount{ 1.0f, 0.0f, 0.0f };
            Check(!resources.UpdateMaterialParameter(material, "BaseColor", wrongComponentCount), "wrong component count should fail");
            Check(updatedBaseRuntime && MappedBufferContainsValue(rhi.GetMappedData(updatedGpu.uboBuffer), *updatedBaseRuntime, updatedBaseColor), "failed update should leave mapped UBO unchanged");

            Check(resources.UpdateMaterialParameter(material, "Metallic", std::array{ 0.9f }), "float update should succeed");
            Check(resources.UpdateMaterialParameter(material, "Roughness", Resource::MaterialParameterValue{ .type = Resource::MaterialParameterType::Float, .value = { 0.65f } }), "typed float update should succeed");
            Check(!resources.UpdateMaterialParameter(material, "Metallic", Resource::MaterialParameterValue{ .type = Resource::MaterialParameterType::Vec4, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }), "typed update should reject mismatched type");
            Check(!resources.UpdateMaterialParameter(material, "MissingParameter", std::array{ 1.0f }), "unknown parameter update should fail");
            Check(resources.GetMaterialParameters(Resource::MaterialHandle::Invalid()).empty(), "invalid material should return empty parameter list");
            Check(!resources.UpdateMaterialParameter(Resource::MaterialHandle::Invalid(), "Metallic", std::array{ 0.1f }), "invalid material update should fail");
        }

        Render::Renderer renderer;
        Check(!renderer.GetOrUploadMaterial(Asset::MaterialHandle::Invalid()).IsValid(), "uninitialized Renderer should reject material upload requests");
        Check(!renderer.CreateMaterialInstance(Resource::MaterialHandle::Invalid()).IsValid(), "uninitialized Renderer should reject material instance creation");
        Check(!renderer.GetOrUploadMaterial(materialAsset).IsValid(), "uninitialized Renderer should not upload valid material assets");
        Check(renderer.GetMaterialParameters(Resource::MaterialHandle::Invalid()).empty(), "uninitialized Renderer should return empty material parameters");
        Check(!renderer.SetMaterialFloat(Resource::MaterialHandle::Invalid(), "Metallic", 0.1f), "uninitialized Renderer should reject material float updates");

        assets.Shutdown();
    }

    std::error_code cleanupError;
    std::filesystem::remove_all(sandboxRoot, cleanupError);

    if (g_failures != 0)
        return 1;

    std::cout << "Material parameter editing checks passed\n";
    return 0;
}
