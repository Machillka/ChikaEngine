#include "TextureDecoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstring>
#include <stb_image.h>

extern "C"
{
#include <exr.h>
}

namespace ChikaEngine::Asset::Internal
{
    namespace
    {
        struct SourcePixels
        {
            uint32_t width = 0;
            uint32_t height = 0;
            TextureSourceEncoding encoding = TextureSourceEncoding::LDR;
            std::vector<uint8_t> unorm8;
            std::vector<float> linear;
        };

        struct SourceDecodeResult
        {
            std::optional<SourcePixels> image;
            TextureLoadStatus status = TextureLoadStatus::DecodeFailed;
            std::string message;
        };

        struct ExrImageGuard
        {
            exr_image image{};

            ~ExrImageGuard()
            {
                exr_image_free(&image);
            }
        };

        std::string Lower(std::string value)
        {
            std::ranges::transform(value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        SourceDecodeResult Fail(TextureLoadStatus status, std::string message)
        {
            return { .image = std::nullopt, .status = status, .message = std::move(message) };
        }

        TextureDecodeResult FinalizeFailure(TextureLoadStatus status, std::string message)
        {
            return { .image = std::nullopt, .status = status, .message = std::move(message) };
        }

        SourceDecodeResult DecodeLDR(const std::filesystem::path& path)
        {
            int width = 0;
            int height = 0;
            int sourceChannels = 0;
            stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &sourceChannels, 4);
            if (!pixels || width <= 0 || height <= 0)
            {
                const std::string reason = stbi_failure_reason() ? stbi_failure_reason() : "stb_image could not decode the image";
                stbi_image_free(pixels);
                return Fail(TextureLoadStatus::DecodeFailed, reason);
            }

            SourcePixels result;
            result.width = static_cast<uint32_t>(width);
            result.height = static_cast<uint32_t>(height);
            result.encoding = TextureSourceEncoding::LDR;
            const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
            result.unorm8.assign(pixels, pixels + byteCount);
            stbi_image_free(pixels);
            return { .image = std::move(result), .status = TextureLoadStatus::Success };
        }

        SourceDecodeResult DecodeRadianceHDR(const std::filesystem::path& path)
        {
            if (stbi_is_hdr(path.string().c_str()) == 0)
                return Fail(TextureLoadStatus::DecodeFailed, "file is not a valid Radiance HDR image");

            int width = 0;
            int height = 0;
            int sourceChannels = 0;
            float* pixels = stbi_loadf(path.string().c_str(), &width, &height, &sourceChannels, 4);
            if (!pixels || width <= 0 || height <= 0)
            {
                const std::string reason = stbi_failure_reason() ? stbi_failure_reason() : "stb_image could not decode the HDR image";
                stbi_image_free(pixels);
                return Fail(TextureLoadStatus::DecodeFailed, reason);
            }

            SourcePixels result;
            result.width = static_cast<uint32_t>(width);
            result.height = static_cast<uint32_t>(height);
            result.encoding = TextureSourceEncoding::RadianceHDR;
            const size_t valueCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
            result.linear.assign(pixels, pixels + valueCount);
            stbi_image_free(pixels);
            return { .image = std::move(result), .status = TextureLoadStatus::Success };
        }

        bool ReadExrChannel(const exr_part& part, int32_t channelIndex, std::vector<float>& values)
        {
            if (channelIndex < 0 || channelIndex >= part.header.num_channels || !part.images || !part.images[channelIndex])
                return false;

            const size_t pixelCount = static_cast<size_t>(part.width) * static_cast<size_t>(part.height);
            values.resize(pixelCount);
            const void* source = part.images[channelIndex];
            switch (part.header.channels[channelIndex].pixel_type)
            {
            case EXR_PIXEL_HALF:
                exr_half_to_float(static_cast<const uint16_t*>(source), values.data(), pixelCount);
                return true;
            case EXR_PIXEL_FLOAT:
                std::memcpy(values.data(), source, pixelCount * sizeof(float));
                return true;
            case EXR_PIXEL_UINT:
            {
                const auto* integers = static_cast<const uint32_t*>(source);
                for (size_t index = 0; index < pixelCount; ++index)
                    values[index] = static_cast<float>(integers[index]);
                return true;
            }
            }
            return false;
        }

        SourceDecodeResult DecodeOpenEXR(const std::filesystem::path& path)
        {
            ExrImageGuard guard;
            const exr_result loadResult = exr_load_from_file(path.string().c_str(), nullptr, &guard.image);
            if (!EXR_OK(loadResult))
            {
                const TextureLoadStatus status = loadResult == EXR_ERROR_UNSUPPORTED ? TextureLoadStatus::UnsupportedEXR : TextureLoadStatus::DecodeFailed;
                return Fail(status, std::string("TinyEXR: ") + exr_result_string(loadResult));
            }
            if (guard.image.num_parts != 1 || !guard.image.parts)
                return Fail(TextureLoadStatus::UnsupportedEXR, "only single-part EXR images are supported");

            const exr_part& part = guard.image.parts[0];
            if (part.is_deep || (part.header.part_type != EXR_PART_SCANLINE && part.header.part_type != EXR_PART_TILED))
                return Fail(TextureLoadStatus::UnsupportedEXR, "deep and volume EXR images are not supported");
            if (part.header.part_type == EXR_PART_TILED && part.header.level_mode != EXR_TILE_ONE_LEVEL)
                return Fail(TextureLoadStatus::UnsupportedEXR, "tiled EXR mipmap/ripmap levels are not supported");
            if (part.width <= 0 || part.height <= 0 || !part.images)
                return Fail(TextureLoadStatus::DecodeFailed, "EXR has invalid dimensions or no flat pixel planes");

            std::array<int32_t, 4> channelIndices{ -1, -1, -1, -1 };
            for (int32_t channelIndex = 0; channelIndex < part.header.num_channels; ++channelIndex)
            {
                const exr_channel& channel = part.header.channels[channelIndex];
                if (channel.x_sampling != 1 || channel.y_sampling != 1)
                    return Fail(TextureLoadStatus::UnsupportedEXR, "subsampled EXR channels are not supported");

                const std::string name = Lower(channel.name);
                const size_t semantic = name == "r" ? 0u : name == "g" ? 1u : name == "b" ? 2u : name == "a" ? 3u : 4u;
                if (semantic < channelIndices.size())
                {
                    if (channelIndices[semantic] >= 0)
                        return Fail(TextureLoadStatus::UnsupportedEXR, "EXR contains duplicate RGBA channel semantics");
                    channelIndices[semantic] = channelIndex;
                }
            }
            if (channelIndices[0] < 0 || channelIndices[1] < 0 || channelIndices[2] < 0)
                return Fail(TextureLoadStatus::UnsupportedEXR, "EXR must contain full-resolution R, G and B channels");

            std::array<std::vector<float>, 4> channels;
            for (size_t semantic = 0; semantic < 3; ++semantic)
            {
                if (!ReadExrChannel(part, channelIndices[semantic], channels[semantic]))
                    return Fail(TextureLoadStatus::UnsupportedEXR, "EXR contains an unsupported RGB channel type");
            }
            if (channelIndices[3] >= 0 && !ReadExrChannel(part, channelIndices[3], channels[3]))
                return Fail(TextureLoadStatus::UnsupportedEXR, "EXR contains an unsupported alpha channel type");

            SourcePixels result;
            result.width = static_cast<uint32_t>(part.width);
            result.height = static_cast<uint32_t>(part.height);
            result.encoding = TextureSourceEncoding::OpenEXR;
            const size_t pixelCount = static_cast<size_t>(part.width) * static_cast<size_t>(part.height);
            result.linear.resize(pixelCount * 4u);
            for (size_t pixel = 0; pixel < pixelCount; ++pixel)
            {
                result.linear[pixel * 4u + 0u] = channels[0][pixel];
                result.linear[pixel * 4u + 1u] = channels[1][pixel];
                result.linear[pixel * 4u + 2u] = channels[2][pixel];
                result.linear[pixel * 4u + 3u] = channelIndices[3] >= 0 ? channels[3][pixel] : 1.0f;
            }
            return { .image = std::move(result), .status = TextureLoadStatus::Success };
        }

        SourceDecodeResult DecodeSource(const std::filesystem::path& path)
        {
            const TextureSourceEncoding encoding = TextureSourceEncodingFromPath(path);
            if (encoding == TextureSourceEncoding::OpenEXR)
                return DecodeOpenEXR(path);
            if (encoding == TextureSourceEncoding::RadianceHDR || stbi_is_hdr(path.string().c_str()) != 0)
                return DecodeRadianceHDR(path);
            return DecodeLDR(path);
        }
    } // namespace

    std::optional<TextureStorageRequest> ParseTextureStorageRequest(const std::string& value)
    {
        const std::string lower = Lower(value);
        if (lower == "auto")
            return TextureStorageRequest::Auto;
        if (lower == "rgba16f")
            return TextureStorageRequest::Float16;
        if (lower == "rgba32f")
            return TextureStorageRequest::Float32;
        return std::nullopt;
    }

    TextureSourceEncoding TextureSourceEncodingFromPath(const std::filesystem::path& path)
    {
        const std::string extension = Lower(path.extension().string());
        if (extension == ".exr")
            return TextureSourceEncoding::OpenEXR;
        if (extension == ".hdr")
            return TextureSourceEncoding::RadianceHDR;
        return TextureSourceEncoding::LDR;
    }

    TextureDecodeResult DecodeTextureImage(const std::filesystem::path& path, TextureStorageRequest request, bool srgb)
    {
        SourceDecodeResult source = DecodeSource(path);
        if (!source.image)
            return FinalizeFailure(source.status, source.message);

        TexturePixelStorage storage = TexturePixelStorage::Float16;
        if (request == TextureStorageRequest::Float32)
            storage = TexturePixelStorage::Float32;
        else if (request == TextureStorageRequest::Auto && source.image->encoding == TextureSourceEncoding::LDR)
            storage = TexturePixelStorage::UNorm8;
        if (storage != TexturePixelStorage::UNorm8 && srgb)
            return FinalizeFailure(TextureLoadStatus::InvalidDescriptor, "floating-point textures must use srgb=false");

        const size_t valueCount = static_cast<size_t>(source.image->width) * static_cast<size_t>(source.image->height) * 4u;
        DecodedTextureImage result;
        result.width = source.image->width;
        result.height = source.image->height;
        result.storage = storage;
        result.encoding = source.image->encoding;

        if (storage == TexturePixelStorage::UNorm8)
        {
            if (source.image->unorm8.size() != valueCount)
                return FinalizeFailure(TextureLoadStatus::InvalidPayloadLayout, "UNorm8 decode result has an invalid byte count");
            result.pixels = std::move(source.image->unorm8);
        }
        else
        {
            std::vector<float> linear = std::move(source.image->linear);
            if (linear.empty())
            {
                if (source.image->unorm8.size() != valueCount)
                    return FinalizeFailure(TextureLoadStatus::InvalidPayloadLayout, "LDR decode result has an invalid byte count");
                linear.resize(valueCount);
                for (size_t index = 0; index < valueCount; ++index)
                    linear[index] = static_cast<float>(source.image->unorm8[index]) / 255.0f;
            }
            if (linear.size() != valueCount)
                return FinalizeFailure(TextureLoadStatus::InvalidPayloadLayout, "floating-point decode result has an invalid value count");

            for (const float value : linear)
            {
                if (!std::isfinite(value))
                    return FinalizeFailure(TextureLoadStatus::InvalidFloatPayload, "floating-point texture contains NaN or infinity");
                if (storage == TexturePixelStorage::Float16 && std::abs(value) > 65504.0f)
                    return FinalizeFailure(TextureLoadStatus::InvalidFloatPayload, "value exceeds Float16 range; request format=rgba32f");
            }

            if (storage == TexturePixelStorage::Float16)
            {
                std::vector<uint16_t> half(valueCount);
                exr_float_to_half(linear.data(), half.data(), valueCount);
                result.pixels.resize(valueCount * sizeof(uint16_t));
                std::memcpy(result.pixels.data(), half.data(), result.pixels.size());
            }
            else
            {
                result.pixels.resize(valueCount * sizeof(float));
                std::memcpy(result.pixels.data(), linear.data(), result.pixels.size());
            }
        }

        uint64_t totalBytes = 0;
        if (!ComputeTexturePayloadLayout(result.width, result.height, 4, 1, result.storage, result.rowBytes, result.layerBytes, totalBytes) || totalBytes != result.pixels.size())
            return FinalizeFailure(TextureLoadStatus::InvalidPayloadLayout, "decoded texture byte layout is inconsistent");
        return { .image = std::move(result), .status = TextureLoadStatus::Success };
    }
} // namespace ChikaEngine::Asset::Internal
