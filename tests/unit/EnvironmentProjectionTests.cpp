#include "ChikaEngine/EnvironmentProjection.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

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

    bool NearlyEqual(float lhs, float rhs, float epsilon = 0.08f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    Asset::TextureData MakeDirectionFixture(uint32_t width = 64, uint32_t height = 32)
    {
        constexpr float PI = 3.14159265358979323846f;
        Asset::TextureData texture;
        texture.path = "procedural-direction-fixture";
        texture.width = width;
        texture.height = height;
        texture.channels = 4;
        texture.srgb = false;
        texture.mipLevels = 1;
        texture.arrayLayers = 1;
        texture.usage = Asset::TextureAssetUsage::Environment;
        texture.shape = Asset::TextureShape::Texture2D;
        texture.pixelStorage = Asset::TexturePixelStorage::Float32;
        texture.sourceEncoding = Asset::TextureSourceEncoding::OpenEXR;
        uint64_t totalBytes = 0;
        Asset::ComputeTexturePayloadLayout(width, height, 4, 1, texture.pixelStorage, texture.rowBytes, texture.layerBytes, totalBytes);
        texture.pixels.resize(static_cast<size_t>(totalBytes));

        for (uint32_t y = 0; y < height; ++y)
        {
            const float latitude = PI * 0.5f - (static_cast<float>(y) + 0.5f) / height * PI;
            for (uint32_t x = 0; x < width; ++x)
            {
                const float longitude = (static_cast<float>(x) + 0.5f) / width * 2.0f * PI - PI;
                const float cosLatitude = std::cos(latitude);
                const std::array<float, 4> value{
                    std::cos(longitude) * cosLatitude * 0.5f + 0.5f,
                    std::sin(latitude) * 0.5f + 0.5f,
                    std::sin(longitude) * cosLatitude * 0.5f + 0.5f,
                    4.0f,
                };
                const size_t texel = static_cast<size_t>(y) * width + x;
                std::memcpy(texture.pixels.data() + texel * sizeof(value), value.data(), sizeof(value));
            }
        }
        return texture;
    }

    std::array<float, 4> ReadTexel(const Asset::TextureData& texture, uint32_t face, uint32_t x, uint32_t y)
    {
        const size_t texel = static_cast<size_t>(face) * texture.width * texture.height + static_cast<size_t>(y) * texture.width + x;
        std::array<float, 4> value{};
        std::memcpy(value.data(), texture.pixels.data() + texel * sizeof(value), sizeof(value));
        return value;
    }

    void TestOrientationAndDynamicRange()
    {
        const Asset::TextureData source = MakeDirectionFixture();
        Asset::EnvironmentProjectionResult converted = Asset::ConvertEquirectangularToCubemap(source, { .outputFaceSize = 17 });
        Check(static_cast<bool>(converted), "valid 2:1 float source converts");
        if (!converted)
            return;

        const Asset::TextureData& cube = *converted.texture;
        Check(cube.shape == Asset::TextureShape::TextureCube && cube.arrayLayers == 6, "projection creates a six-layer Cubemap");
        Check(cube.width == 17 && cube.height == 17 && cube.pixelStorage == Asset::TexturePixelStorage::Float32, "projection preserves requested size and float storage");
        const uint32_t center = cube.width / 2;
        const std::array<std::array<float, 3>, 6> expected{
            std::array{ 1.0f, 0.5f, 0.5f }, std::array{ 0.0f, 0.5f, 0.5f }, std::array{ 0.5f, 1.0f, 0.5f }, std::array{ 0.5f, 0.0f, 0.5f }, std::array{ 0.5f, 0.5f, 1.0f }, std::array{ 0.5f, 0.5f, 0.0f },
        };
        for (uint32_t face = 0; face < 6; ++face)
        {
            const std::array<float, 4> sampled = ReadTexel(cube, face, center, center);
            Check(NearlyEqual(sampled[0], expected[face][0]) && NearlyEqual(sampled[1], expected[face][1]) && NearlyEqual(sampled[2], expected[face][2]), "Cubemap face center follows px,nx,py,ny,pz,nz orientation");
            Check(sampled[3] > 1.0f, "HDR value above 1.0 survives projection");
        }
    }

    void TestLongitudeSeamWrap()
    {
        const Asset::TextureData source = MakeDirectionFixture(128, 64);
        Asset::EnvironmentProjectionResult converted = Asset::ConvertEquirectangularToCubemap(source, { .outputFaceSize = 32 });
        Check(static_cast<bool>(converted), "seam fixture converts");
        if (!converted)
            return;

        const uint32_t y = converted.texture->height / 2;
        const auto left = ReadTexel(*converted.texture, 1, converted.texture->width / 2 - 1, y);
        const auto right = ReadTexel(*converted.texture, 1, converted.texture->width / 2, y);
        Check(NearlyEqual(left[0], right[0], 0.02f) && NearlyEqual(left[2], right[2], 0.06f), "longitude wrap remains continuous around the -X meridian");
    }

    void TestValidation()
    {
        Asset::TextureData invalidAspect = MakeDirectionFixture(62, 32);
        Check(Asset::ConvertEquirectangularToCubemap(invalidAspect).status == Asset::EnvironmentProjectionStatus::InvalidAspectRatio, "non-2:1 source is rejected");

        const Asset::TextureData valid = MakeDirectionFixture();
        Check(Asset::ConvertEquirectangularToCubemap(valid, { .outputFaceSize = 65, .maxFaceSize = 64 }).status == Asset::EnvironmentProjectionStatus::InvalidFaceSize, "face size above the configured limit is rejected before allocation");

        Asset::TextureData invalidFloat = MakeDirectionFixture();
        const float nan = std::numeric_limits<float>::quiet_NaN();
        std::memcpy(invalidFloat.pixels.data(), &nan, sizeof(nan));
        Check(Asset::ConvertEquirectangularToCubemap(invalidFloat, { .outputFaceSize = 8 }).status == Asset::EnvironmentProjectionStatus::InvalidFloatPayload, "NaN projection input is rejected deterministically");
    }
} // namespace

int main()
{
    TestOrientationAndDynamicRange();
    TestLongitudeSeamWrap();
    TestValidation();
    if (g_failures == 0)
        std::cout << "Environment projection checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
