#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RenderGraphBlackboard.hpp"
#include "ChikaEngine/ResourceLayout.hpp"
#include "ChikaEngine/TextureLoader.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

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
        Check(gpu.usage == Asset::TextureAssetUsage::Environment, "texture gpu stores usage");
        Check(gpu.arrayLayers == 6, "texture gpu stores layer count");
    }
} // namespace

int main()
{
    TestTextureDimensionValidation();
    TestTextureDescriptorLoading();
    TestRenderEnvironmentSemantics();
    TestTextureGpuContract();

    if (g_failures == 0)
        std::cout << "Environment resource contract checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
