#include "ChikaEngine/EnvironmentProjection.hpp"

#include "ChikaEngine/debug/log_macros.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

extern "C"
{
#include <exr.h>
}

namespace ChikaEngine::Asset
{
    namespace
    {
        constexpr float PI = 3.14159265358979323846f;

        EnvironmentProjectionResult Failure(EnvironmentProjectionStatus status, std::string message)
        {
            return { .texture = nullptr, .status = status, .message = std::move(message) };
        }

        float ReadComponent(const TextureData& source, size_t component)
        {
            if (source.pixelStorage == TexturePixelStorage::Float16)
            {
                uint16_t half = 0;
                std::memcpy(&half, source.pixels.data() + component * sizeof(uint16_t), sizeof(half));
                float value = 0.0f;
                exr_half_to_float(&half, &value, 1);
                return value;
            }

            float value = 0.0f;
            std::memcpy(&value, source.pixels.data() + component * sizeof(float), sizeof(value));
            return value;
        }

        std::array<float, 4> ReadTexel(const TextureData& source, uint32_t x, uint32_t y)
        {
            const size_t firstComponent = (static_cast<size_t>(y) * source.width + x) * 4u;
            return {
                ReadComponent(source, firstComponent + 0u),
                ReadComponent(source, firstComponent + 1u),
                ReadComponent(source, firstComponent + 2u),
                ReadComponent(source, firstComponent + 3u),
            };
        }

        std::array<float, 4> SampleEquirectangular(const TextureData& source, float u, float v)
        {
            u -= std::floor(u);
            v = std::clamp(v, 0.0f, 1.0f);

            const float sourceX = u * static_cast<float>(source.width) - 0.5f;
            const float sourceY = v * static_cast<float>(source.height) - 0.5f;
            const int64_t baseX = static_cast<int64_t>(std::floor(sourceX));
            const int64_t baseY = static_cast<int64_t>(std::floor(sourceY));
            const float blendX = sourceX - std::floor(sourceX);
            const float blendY = sourceY - std::floor(sourceY);

            const auto wrapX = [&](int64_t x)
            {
                const int64_t width = source.width;
                return static_cast<uint32_t>((x % width + width) % width);
            };
            const auto clampY = [&](int64_t y) { return static_cast<uint32_t>(std::clamp<int64_t>(y, 0, source.height - 1)); };

            const std::array<float, 4> topLeft = ReadTexel(source, wrapX(baseX), clampY(baseY));
            const std::array<float, 4> topRight = ReadTexel(source, wrapX(baseX + 1), clampY(baseY));
            const std::array<float, 4> bottomLeft = ReadTexel(source, wrapX(baseX), clampY(baseY + 1));
            const std::array<float, 4> bottomRight = ReadTexel(source, wrapX(baseX + 1), clampY(baseY + 1));

            std::array<float, 4> sampled{};
            for (size_t component = 0; component < sampled.size(); ++component)
            {
                const float top = std::lerp(topLeft[component], topRight[component], blendX);
                const float bottom = std::lerp(bottomLeft[component], bottomRight[component], blendX);
                sampled[component] = std::lerp(top, bottom, blendY);
            }
            return sampled;
        }

        void WriteTexel(TextureData& destination, size_t texel, const std::array<float, 4>& value)
        {
            if (destination.pixelStorage == TexturePixelStorage::Float16)
            {
                std::array<uint16_t, 4> half{};
                exr_float_to_half(value.data(), half.data(), half.size());
                std::memcpy(destination.pixels.data() + texel * sizeof(half), half.data(), sizeof(half));
                return;
            }
            std::memcpy(destination.pixels.data() + texel * sizeof(value), value.data(), sizeof(value));
        }
    } // namespace

    std::array<float, 3> CubemapTexelDirection(uint32_t face, float u, float v)
    {
        const float x = u * 2.0f - 1.0f;
        const float y = v * 2.0f - 1.0f;
        std::array<float, 3> direction{};
        switch (face)
        {
        case 0: // +X
            direction = { 1.0f, -y, -x };
            break;
        case 1: // -X
            direction = { -1.0f, -y, x };
            break;
        case 2: // +Y
            direction = { x, 1.0f, y };
            break;
        case 3: // -Y
            direction = { x, -1.0f, -y };
            break;
        case 4: // +Z
            direction = { x, -y, 1.0f };
            break;
        case 5: // -Z
            direction = { -x, -y, -1.0f };
            break;
        default:
            return {};
        }

        const float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
        for (float& component : direction)
            component /= length;
        return direction;
    }

    EnvironmentProjectionResult ConvertEquirectangularToCubemap(const TextureData& source, const EnvironmentProjectionOptions& options)
    {
        const auto started = std::chrono::steady_clock::now();
        if (source.shape != TextureShape::Texture2D || source.arrayLayers != 1 || source.channels != 4 || source.mipLevels != 1 || !IsTexturePayloadLayoutValid(source))
            return Failure(EnvironmentProjectionStatus::InvalidSource, "equirectangular source must be a valid single-layer RGBA mip-0 texture");
        if (source.pixelStorage != TexturePixelStorage::Float16 && source.pixelStorage != TexturePixelStorage::Float32)
            return Failure(EnvironmentProjectionStatus::InvalidSource, "equirectangular environment conversion requires Float16 or Float32 source pixels");
        if (source.srgb)
            return Failure(EnvironmentProjectionStatus::InvalidSource, "equirectangular environment source must be linear (srgb=false)");
        const size_t componentCount = static_cast<size_t>(source.width) * source.height * source.channels;
        for (size_t component = 0; component < componentCount; ++component)
        {
            if (!std::isfinite(ReadComponent(source, component)))
                return Failure(EnvironmentProjectionStatus::InvalidFloatPayload, "equirectangular source contains NaN or infinity");
        }

        const uint64_t expectedWidth = static_cast<uint64_t>(source.height) * 2u;
        const uint64_t aspectDifference = source.width > expectedWidth ? source.width - expectedWidth : expectedWidth - source.width;
        if (aspectDifference > 1u)
            return Failure(EnvironmentProjectionStatus::InvalidAspectRatio, "equirectangular source aspect ratio must be 2:1 (within one pixel)");

        const uint32_t faceSize = options.outputFaceSize == 0 ? source.width / 4u : options.outputFaceSize;
        const uint32_t maximum = std::min(options.maxFaceSize, MAX_ENVIRONMENT_CUBEMAP_FACE_SIZE);
        if (faceSize == 0 || maximum == 0 || faceSize > maximum)
            return Failure(EnvironmentProjectionStatus::InvalidFaceSize, "outputFaceSize must be non-zero and within the configured Cubemap limit");

        auto destination = std::make_unique<TextureData>();
        destination->path = source.path;
        destination->width = faceSize;
        destination->height = faceSize;
        destination->channels = 4;
        destination->srgb = false;
        destination->generateMips = false;
        destination->mipLevels = 1;
        destination->arrayLayers = 6;
        destination->usage = source.usage;
        destination->shape = TextureShape::TextureCube;
        destination->fallback = source.fallback;
        destination->pixelStorage = source.pixelStorage;
        destination->sourceEncoding = source.sourceEncoding;

        uint64_t totalBytes = 0;
        if (!ComputeTexturePayloadLayout(faceSize, faceSize, 4, 6, destination->pixelStorage, destination->rowBytes, destination->layerBytes, totalBytes) || totalBytes > std::numeric_limits<size_t>::max())
            return Failure(EnvironmentProjectionStatus::InvalidFaceSize, "Cubemap output byte size overflows the platform address space");
        destination->pixels.resize(static_cast<size_t>(totalBytes));

        for (uint32_t face = 0; face < 6; ++face)
        {
            for (uint32_t y = 0; y < faceSize; ++y)
            {
                for (uint32_t x = 0; x < faceSize; ++x)
                {
                    const std::array<float, 3> direction = CubemapTexelDirection(face, (static_cast<float>(x) + 0.5f) / faceSize, (static_cast<float>(y) + 0.5f) / faceSize);
                    const float longitude = std::atan2(direction[2], direction[0]);
                    const float latitude = std::asin(std::clamp(direction[1], -1.0f, 1.0f));
                    const float u = longitude / (2.0f * PI) + 0.5f;
                    const float v = 0.5f - latitude / PI;
                    const std::array<float, 4> sampled = SampleEquirectangular(source, u, v);
                    if (!std::ranges::all_of(sampled, [](float value) { return std::isfinite(value); }))
                        return Failure(EnvironmentProjectionStatus::InvalidFloatPayload, "projection produced NaN or infinity from the source payload");
                    const size_t outputTexel = (static_cast<size_t>(face) * faceSize * faceSize) + static_cast<size_t>(y) * faceSize + x;
                    WriteTexel(*destination, outputTexel, sampled);
                }
            }
        }

        const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        LOG_INFO("EnvironmentProjection", "Converted '{}' {}x{} equirectangular to {}x{}x6 {} Cubemap in {:.2f} ms ({} bytes)", source.path, source.width, source.height, faceSize, faceSize, TexturePixelStorageName(source.pixelStorage), milliseconds, totalBytes);
        return {
            .texture = std::move(destination),
            .status = EnvironmentProjectionStatus::Success,
            .conversionMilliseconds = milliseconds,
        };
    }
} // namespace ChikaEngine::Asset
