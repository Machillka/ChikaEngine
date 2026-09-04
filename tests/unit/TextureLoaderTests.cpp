#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/TextureLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

extern "C"
{
#include <exr.h>
}

namespace
{
    namespace Asset = ChikaEngine::Asset;

    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }

    bool NearlyEqual(float lhs, float rhs, float epsilon = 0.01f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

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

    bool WriteRadianceHDR(const std::filesystem::path& path, uint32_t width, uint32_t height, const std::vector<std::array<float, 3>>& pixels)
    {
        if (pixels.size() != static_cast<size_t>(width) * height)
            return false;
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y " << height << " +X " << width << '\n';
        for (const auto& pixel : pixels)
        {
            const std::array<uint8_t, 4> rgbe = EncodeRGBE(pixel);
            file.write(reinterpret_cast<const char*>(rgbe.data()), static_cast<std::streamsize>(rgbe.size()));
        }
        return file.good();
    }

    bool WriteEXR(const std::filesystem::path& path, uint32_t width, uint32_t height, const std::vector<float>& red, const std::vector<float>& green, const std::vector<float>& blue, const std::optional<std::vector<float>>& alpha = std::nullopt)
    {
        const size_t pixelCount = static_cast<size_t>(width) * height;
        if (red.size() != pixelCount || green.size() != pixelCount || blue.size() != pixelCount || (alpha && alpha->size() != pixelCount))
            return false;

        const std::array<const char*, 4> namesWithAlpha{ "A", "B", "G", "R" };
        const std::array<const char*, 3> namesWithoutAlpha{ "B", "G", "R" };
        std::vector<exr_channel> channels(alpha ? 4u : 3u);
        std::vector<void*> planes(alpha ? 4u : 3u);
        for (size_t index = 0; index < channels.size(); ++index)
        {
            const char* name = alpha ? namesWithAlpha[index] : namesWithoutAlpha[index];
            channels[index].name[0] = name[0];
            channels[index].name[1] = '\0';
            channels[index].pixel_type = EXR_PIXEL_FLOAT;
            channels[index].x_sampling = 1;
            channels[index].y_sampling = 1;
        }
        if (alpha)
            planes = { const_cast<float*>(alpha->data()), const_cast<float*>(blue.data()), const_cast<float*>(green.data()), const_cast<float*>(red.data()) };
        else
            planes = { const_cast<float*>(blue.data()), const_cast<float*>(green.data()), const_cast<float*>(red.data()) };

        exr_part part{};
        part.header.part_type = EXR_PART_SCANLINE;
        part.header.data_window = { 0, 0, static_cast<int32_t>(width) - 1, static_cast<int32_t>(height) - 1 };
        part.header.display_window = part.header.data_window;
        part.header.pixel_aspect_ratio = 1.0f;
        part.header.screen_window_width = 1.0f;
        part.header.num_channels = static_cast<int32_t>(channels.size());
        part.header.channels = channels.data();
        part.width = static_cast<int32_t>(width);
        part.height = static_cast<int32_t>(height);
        part.images = planes.data();

        exr_image image{};
        image.num_parts = 1;
        image.parts = &part;
        return EXR_OK(exr_save_to_file(path.string().c_str(), &image, EXR_COMPRESSION_NONE));
    }

    bool WriteUnsupportedLuminanceEXR(const std::filesystem::path& path)
    {
        float luminance = 1.0f;
        exr_channel channel{};
        channel.name[0] = 'Y';
        channel.pixel_type = EXR_PIXEL_FLOAT;
        channel.x_sampling = 1;
        channel.y_sampling = 1;
        void* plane = &luminance;

        exr_part part{};
        part.header.part_type = EXR_PART_SCANLINE;
        part.header.data_window = { 0, 0, 0, 0 };
        part.header.display_window = part.header.data_window;
        part.header.pixel_aspect_ratio = 1.0f;
        part.header.screen_window_width = 1.0f;
        part.header.num_channels = 1;
        part.header.channels = &channel;
        part.width = 1;
        part.height = 1;
        part.images = &plane;

        exr_image image{};
        image.num_parts = 1;
        image.parts = &part;
        return EXR_OK(exr_save_to_file(path.string().c_str(), &image, EXR_COMPRESSION_NONE));
    }

    std::array<float, 4> ReadTexel(const Asset::TextureData& texture, size_t pixel)
    {
        std::array<float, 4> result{};
        const size_t valueOffset = pixel * 4u;
        if (texture.pixelStorage == Asset::TexturePixelStorage::Float16)
        {
            std::array<uint16_t, 4> half{};
            std::memcpy(half.data(), texture.pixels.data() + valueOffset * sizeof(uint16_t), sizeof(half));
            exr_half_to_float(half.data(), result.data(), result.size());
        }
        else if (texture.pixelStorage == Asset::TexturePixelStorage::Float32)
            std::memcpy(result.data(), texture.pixels.data() + valueOffset * sizeof(float), sizeof(result));
        else
        {
            for (size_t channel = 0; channel < 4; ++channel)
                result[channel] = static_cast<float>(texture.pixels[valueOffset + channel]) / 255.0f;
        }
        return result;
    }

    std::string Quote(const std::filesystem::path& path)
    {
        return "\"" + path.generic_string() + "\"";
    }

    std::string CubeDescriptor(const std::array<std::filesystem::path, 6>& faces, std::string_view format = "rgba32f", bool srgb = false)
    {
        std::ostringstream descriptor;
        descriptor << "{\n"
                   << "  \"usage\": \"environment\",\n"
                   << "  \"format\": \"" << format << "\",\n"
                   << "  \"srgb\": " << (srgb ? "true" : "false") << ",\n"
                   << "  \"generateMips\": false,\n"
                   << "  \"cubeFaces\": {\n"
                   << "    \"px\": " << Quote(faces[0]) << ",\n"
                   << "    \"nx\": " << Quote(faces[1]) << ",\n"
                   << "    \"py\": " << Quote(faces[2]) << ",\n"
                   << "    \"ny\": " << Quote(faces[3]) << ",\n"
                   << "    \"pz\": " << Quote(faces[4]) << ",\n"
                   << "    \"nz\": " << Quote(faces[5]) << "\n"
                   << "  }\n"
                   << "}\n";
        return descriptor.str();
    }

    void TestHDRAndDescriptorFormats(const std::filesystem::path& root)
    {
        const std::filesystem::path hdr = root / "dynamic-range.hdr";
        Check(WriteRadianceHDR(hdr, 2, 1, { { 4.0f, 0.5f, 0.25f }, { 1.0f, 2.0f, 8.0f } }), "HDR fixture is written");

        Asset::TextureLoadResult automatic = Asset::TextureLoader::LoadWithStatus(hdr.string());
        Check(static_cast<bool>(automatic), "direct HDR loads");
        if (automatic)
        {
            Check(automatic.texture->sourceEncoding == Asset::TextureSourceEncoding::RadianceHDR, "HDR source encoding is recorded");
            Check(automatic.texture->pixelStorage == Asset::TexturePixelStorage::Float16, "HDR auto format selects Float16");
            Check(automatic.texture->rowBytes == 16 && automatic.texture->layerBytes == 16 && automatic.texture->pixels.size() == 16, "HDR Float16 layout is exact");
            Check(ReadTexel(*automatic.texture, 0)[0] > 1.0f, "HDR value above 1.0 survives Float16 decode");
        }

        const std::filesystem::path descriptor = root / "dynamic-range.texture";
        Check(WriteText(descriptor, std::string("{\"source\":") + Quote(hdr) + ",\"format\":\"rgba32f\",\"srgb\":false}"), "HDR rgba32f descriptor is written");
        Asset::TextureLoadResult fullPrecision = Asset::TextureLoader::LoadWithStatus(descriptor.string());
        Check(static_cast<bool>(fullPrecision), "HDR rgba32f descriptor loads");
        if (fullPrecision)
        {
            Check(fullPrecision.texture->pixelStorage == Asset::TexturePixelStorage::Float32, "rgba32f descriptor selects Float32");
            Check(fullPrecision.texture->rowBytes == 32 && fullPrecision.texture->pixels.size() == 32, "HDR Float32 layout is exact");
            Check(ReadTexel(*fullPrecision.texture, 1)[2] > 1.0f, "HDR high blue value survives Float32 payload");
        }

        const std::filesystem::path invalidSrgb = root / "invalid-srgb.texture";
        Check(WriteText(invalidSrgb, std::string("{\"source\":") + Quote(hdr) + ",\"format\":\"rgba16f\",\"srgb\":true}"), "invalid HDR sRGB descriptor is written");
        const Asset::TextureLoadResult rejected = Asset::TextureLoader::LoadWithStatus(invalidSrgb.string());
        Check(!rejected && rejected.status == Asset::TextureLoadStatus::InvalidDescriptor, "floating-point srgb=true is rejected explicitly");
    }

    void TestEXRChannelsAndInvalidValues(const std::filesystem::path& root)
    {
        const std::filesystem::path exr = root / "channels.exr";
        Check(WriteEXR(exr, 2, 1, { 2.0f, 4.0f }, { -0.25f, 0.5f }, { 0.125f, 8.0f }), "EXR fixture without alpha is written");
        const std::filesystem::path descriptor = root / "channels.texture";
        Check(WriteText(descriptor, std::string("{\"source\":") + Quote(exr) + ",\"format\":\"rgba32f\",\"srgb\":false}"), "EXR descriptor is written");

        Asset::TextureLoadResult loaded = Asset::TextureLoader::LoadWithStatus(descriptor.string());
        Check(static_cast<bool>(loaded), "EXR RGB fixture loads");
        if (loaded)
        {
            Check(loaded.texture->sourceEncoding == Asset::TextureSourceEncoding::OpenEXR, "EXR source encoding is recorded");
            Check(loaded.texture->width == 2 && loaded.texture->height == 1, "EXR dimensions are preserved");
            const auto first = ReadTexel(*loaded.texture, 0);
            Check(NearlyEqual(first[0], 2.0f) && NearlyEqual(first[1], -0.25f) && NearlyEqual(first[2], 0.125f), "EXR RGB channels and finite negative values are preserved");
            Check(NearlyEqual(first[3], 1.0f), "missing EXR alpha defaults to 1.0");
        }
        Check(Asset::AssetDatabase::Classify(exr) == Asset::AssetType::Texture, "AssetDatabase classifies .exr as Texture");

        const std::filesystem::path invalid = root / "invalid-float.exr";
        Check(WriteEXR(invalid, 1, 1, { std::numeric_limits<float>::quiet_NaN() }, { 0.0f }, { 0.0f }), "invalid EXR fixture is written");
        const Asset::TextureLoadResult rejected = Asset::TextureLoader::LoadWithStatus(invalid.string());
        Check(!rejected && rejected.status == Asset::TextureLoadStatus::InvalidFloatPayload, "NaN EXR payload is rejected with a stable status");

        const std::filesystem::path unsupported = root / "luminance.exr";
        Check(WriteUnsupportedLuminanceEXR(unsupported), "unsupported EXR fixture is written");
        const Asset::TextureLoadResult unsupportedResult = Asset::TextureLoader::LoadWithStatus(unsupported.string());
        Check(!unsupportedResult && unsupportedResult.status == Asset::TextureLoadStatus::UnsupportedEXR, "unsupported EXR channel layout has a stable status");
    }

    void TestCubemapValidation(const std::filesystem::path& root)
    {
        std::array<std::filesystem::path, 6> hdrFaces;
        for (size_t face = 0; face < hdrFaces.size(); ++face)
        {
            hdrFaces[face] = root / ("face-" + std::to_string(face) + ".hdr");
            const float value = 2.0f + static_cast<float>(face);
            Check(WriteRadianceHDR(hdrFaces[face], 1, 1, { { value, 0.0f, 0.0f } }), "HDR cubemap face is written");
        }

        const std::filesystem::path cube = root / "ordered.texture";
        Check(WriteText(cube, CubeDescriptor(hdrFaces)), "HDR Cubemap descriptor is written");
        Asset::TextureLoadResult loaded = Asset::TextureLoader::LoadWithStatus(cube.string());
        Check(static_cast<bool>(loaded), "six-face HDR Cubemap loads");
        if (loaded)
        {
            Check(loaded.texture->shape == Asset::TextureShape::TextureCube && loaded.texture->arrayLayers == 6, "HDR descriptor creates a six-layer Cubemap");
            bool faceOrderMatches = loaded.texture->cubeFaces.size() == hdrFaces.size();
            for (size_t face = 0; face < hdrFaces.size() && faceOrderMatches; ++face)
                faceOrderMatches = loaded.texture->cubeFaces[face] == hdrFaces[face].string();
            Check(faceOrderMatches, "Cubemap keeps px,nx,py,ny,pz,nz face order");
            for (size_t face = 0; face < hdrFaces.size(); ++face)
                Check(NearlyEqual(ReadTexel(*loaded.texture, face)[0], 2.0f + static_cast<float>(face)), "Cubemap layer payload follows descriptor face order");
            Check(loaded.texture->layerBytes == 16 && loaded.texture->pixels.size() == 96, "RGBA32F Cubemap staging layout covers all layers");
        }

        std::array<std::filesystem::path, 6> mismatched = hdrFaces;
        mismatched[5] = root / "wide.hdr";
        Check(WriteRadianceHDR(mismatched[5], 2, 1, { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }), "mismatched HDR face is written");
        const std::filesystem::path mismatchedDescriptor = root / "mismatched.texture";
        Check(WriteText(mismatchedDescriptor, CubeDescriptor(mismatched)), "mismatched Cubemap descriptor is written");
        Check(Asset::TextureLoader::LoadWithStatus(mismatchedDescriptor.string()).status == Asset::TextureLoadStatus::InvalidDescriptor, "Cubemap rejects mismatched dimensions");

        std::array<std::filesystem::path, 6> mixed = hdrFaces;
        mixed[5] = root / "mixed.exr";
        Check(WriteEXR(mixed[5], 1, 1, { 1.0f }, { 1.0f }, { 1.0f }), "mixed EXR face is written");
        const std::filesystem::path mixedDescriptor = root / "mixed.texture";
        Check(WriteText(mixedDescriptor, CubeDescriptor(mixed)), "mixed Cubemap descriptor is written");
        Check(Asset::TextureLoader::LoadWithStatus(mixedDescriptor.string()).status == Asset::TextureLoadStatus::InvalidDescriptor, "Cubemap rejects mixed HDR and EXR encodings");

        const std::filesystem::path fakeMips = root / "fake-mips.texture";
        Check(WriteText(fakeMips, std::string("{\"source\":") + Quote(hdrFaces[0]) + ",\"srgb\":false,\"generateMips\":true}"), "fake mip descriptor is written");
        Check(Asset::TextureLoader::LoadWithStatus(fakeMips.string()).status == Asset::TextureLoadStatus::InvalidDescriptor, "descriptor rejects mip generation until it is implemented");
    }

    void TestEquirectangularDescriptors(const std::filesystem::path& root)
    {
        std::vector<std::array<float, 3>> hdrPixels(32, { 2.0f, 0.5f, 4.0f });
        const std::filesystem::path hdr = root / "panorama.hdr";
        Check(WriteRadianceHDR(hdr, 8, 4, hdrPixels), "2:1 equirectangular HDR fixture is written");
        const std::filesystem::path hdrDescriptor = root / "panorama-hdr.texture";
        Check(WriteText(hdrDescriptor, std::string("{\"usage\":\"environment\",\"source\":") + Quote(hdr) + ",\"projection\":\"equirectangular\",\"outputFaceSize\":4,\"format\":\"rgba16f\",\"srgb\":false}"), "equirectangular HDR descriptor is written");
        Asset::TextureLoadResult hdrCube = Asset::TextureLoader::LoadWithStatus(hdrDescriptor.string());
        Check(static_cast<bool>(hdrCube), "equirectangular HDR descriptor loads");
        if (hdrCube)
        {
            Check(hdrCube.texture->shape == Asset::TextureShape::TextureCube && hdrCube.texture->arrayLayers == 6, "equirectangular HDR produces a Cubemap");
            Check(hdrCube.texture->projection == Asset::TextureProjection::Equirectangular && hdrCube.texture->sourcePath == hdr.string(), "projection and source diagnostics are retained");
            Check(hdrCube.texture->width == 4 && hdrCube.texture->pixelStorage == Asset::TexturePixelStorage::Float16, "HDR projection honors outputFaceSize and rgba16f");
            Check(ReadTexel(*hdrCube.texture, 0)[0] > 1.0f && hdrCube.texture->pixels.size() == 4u * 4u * 6u * 8u, "HDR projection preserves radiance and exact six-layer layout");
        }

        const size_t pixelCount = 32;
        const std::filesystem::path exr = root / "panorama.exr";
        Check(WriteEXR(exr, 8, 4, std::vector<float>(pixelCount, 8.0f), std::vector<float>(pixelCount, 1.0f), std::vector<float>(pixelCount, 0.25f)), "2:1 equirectangular EXR fixture is written");
        const std::filesystem::path exrDescriptor = root / "panorama-exr.texture";
        Check(WriteText(exrDescriptor, std::string("{\"usage\":\"environment\",\"source\":") + Quote(exr) + ",\"projection\":\"equirectangular\",\"format\":\"rgba32f\",\"srgb\":false}"), "equirectangular EXR descriptor is written");
        Asset::TextureLoadResult exrCube = Asset::TextureLoader::LoadWithStatus(exrDescriptor.string());
        Check(static_cast<bool>(exrCube), "equirectangular EXR descriptor loads");
        if (exrCube)
            Check(exrCube.texture->width == 2 && exrCube.texture->pixelStorage == Asset::TexturePixelStorage::Float32 && ReadTexel(*exrCube.texture, 0)[0] > 1.0f, "EXR projection uses sourceWidth/4 default and preserves HDR values");

        const std::filesystem::path invalidAspect = root / "invalid-aspect.hdr";
        Check(WriteRadianceHDR(invalidAspect, 6, 4, std::vector<std::array<float, 3>>(24, { 1.0f, 1.0f, 1.0f })), "invalid aspect fixture is written");
        const std::filesystem::path invalidDescriptor = root / "invalid-aspect.texture";
        Check(WriteText(invalidDescriptor, std::string("{\"source\":") + Quote(invalidAspect) + ",\"projection\":\"equirectangular\",\"srgb\":false}"), "invalid aspect descriptor is written");
        Check(Asset::TextureLoader::LoadWithStatus(invalidDescriptor.string()).status == Asset::TextureLoadStatus::InvalidProjection, "invalid equirectangular aspect has a stable status");

        const std::filesystem::path oversizedDescriptor = root / "oversized.texture";
        Check(WriteText(oversizedDescriptor, std::string("{\"source\":") + Quote(hdr) + ",\"projection\":\"equirectangular\",\"outputFaceSize\":16385,\"srgb\":false}"), "oversized descriptor is written");
        Check(Asset::TextureLoader::LoadWithStatus(oversizedDescriptor.string()).status == Asset::TextureLoadStatus::FaceSizeLimitExceeded, "absolute Cubemap face limit is enforced before allocation");
    }
} // namespace

int main()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "chika_texture_loader_tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    Check(!error, "texture fixture directory is created");

    TestHDRAndDescriptorFormats(root);
    TestEXRChannelsAndInvalidValues(root);
    TestCubemapValidation(root);
    TestEquirectangularDescriptors(root);

    std::filesystem::remove_all(root, error);
    if (g_failures == 0)
        std::cout << "Texture loader HDR/EXR checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
