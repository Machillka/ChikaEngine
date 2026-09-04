#pragma once

#include "ChikaEngine/AssetLayouts.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ChikaEngine::Asset::Internal
{
    enum class TextureStorageRequest : uint8_t
    {
        Auto,
        Float16,
        Float32,
    };

    struct DecodedTextureImage
    {
        uint32_t width = 0;
        uint32_t height = 0;
        TexturePixelStorage storage = TexturePixelStorage::UNorm8;
        TextureSourceEncoding encoding = TextureSourceEncoding::LDR;
        uint64_t rowBytes = 0;
        uint64_t layerBytes = 0;
        std::vector<uint8_t> pixels;
    };

    struct TextureDecodeResult
    {
        std::optional<DecodedTextureImage> image;
        TextureLoadStatus status = TextureLoadStatus::DecodeFailed;
        std::string message;
    };

    std::optional<TextureStorageRequest> ParseTextureStorageRequest(const std::string& value);
    TextureSourceEncoding TextureSourceEncodingFromPath(const std::filesystem::path& path);
    TextureDecodeResult DecodeTextureImage(const std::filesystem::path& path, TextureStorageRequest request, bool srgb);
} // namespace ChikaEngine::Asset::Internal
