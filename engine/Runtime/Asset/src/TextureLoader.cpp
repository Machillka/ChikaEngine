#include "ChikaEngine/TextureLoader.hpp"
#include "ChikaEngine/EnvironmentProjection.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "TextureDecoder.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace ChikaEngine::Asset
{
    namespace
    {
        std::string Lower(std::string value)
        {
            std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        TextureAssetUsage ParseUsage(const std::string& value)
        {
            const std::string lower = Lower(value);
            if (lower == "data")
                return TextureAssetUsage::Data;
            if (lower == "environment")
                return TextureAssetUsage::Environment;
            if (lower == "environment_irradiance" || lower == "irradiance")
                return TextureAssetUsage::EnvironmentIrradiance;
            if (lower == "environment_prefiltered" || lower == "prefiltered")
                return TextureAssetUsage::EnvironmentPrefiltered;
            if (lower == "environment_brdf_lut" || lower == "brdf_lut")
                return TextureAssetUsage::EnvironmentBrdfLut;
            if (lower == "reflection_probe")
                return TextureAssetUsage::ReflectionProbe;
            return TextureAssetUsage::Color;
        }

        TextureFallback ParseFallback(const std::string& value)
        {
            const std::string lower = Lower(value);
            if (lower == "gray_irradiance")
                return TextureFallback::GrayIrradiance;
            if (lower == "black_prefilter")
                return TextureFallback::BlackPrefilter;
            if (lower == "brdf_lut")
                return TextureFallback::BrdfLut;
            return TextureFallback::None;
        }

        std::filesystem::path ResolvePath(const std::filesystem::path& baseDir, const std::string& value)
        {
            std::filesystem::path path(value);
            if (path.is_relative())
                path = baseDir / path;
            return path.lexically_normal();
        }

        TextureLoadResult Failure(TextureLoadStatus status, const std::filesystem::path& path, std::string message)
        {
            LOG_ERROR("TextureLoader", "Texture load failed path='{}' status={} reason='{}'", path.string(), static_cast<uint32_t>(status), message);
            return { .texture = nullptr, .status = status, .message = std::move(message) };
        }

        std::unique_ptr<TextureData> CreateFallbackTexture(const std::string& path, TextureAssetUsage requestedUsage, TextureFallback fallback)
        {
            if (fallback == TextureFallback::None)
                return nullptr;

            auto texture = std::make_unique<TextureData>();
            texture->path = path;
            texture->width = 1;
            texture->height = 1;
            texture->channels = 4;
            texture->srgb = false;
            texture->mipLevels = 1;
            texture->usage = requestedUsage;
            texture->fallback = fallback;
            texture->pixelStorage = TexturePixelStorage::UNorm8;
            texture->sourceEncoding = TextureSourceEncoding::Generated;
            texture->rowBytes = 4;
            texture->layerBytes = 4;

            std::array<uint8_t, 4> color{ 0, 0, 0, 255 };
            switch (fallback)
            {
            case TextureFallback::GrayIrradiance:
                texture->usage = requestedUsage == TextureAssetUsage::Color ? TextureAssetUsage::EnvironmentIrradiance : requestedUsage;
                texture->shape = TextureShape::TextureCube;
                texture->arrayLayers = 6;
                color = { 128, 128, 128, 255 };
                break;
            case TextureFallback::BlackPrefilter:
                texture->usage = requestedUsage == TextureAssetUsage::Color ? TextureAssetUsage::EnvironmentPrefiltered : requestedUsage;
                texture->shape = TextureShape::TextureCube;
                texture->arrayLayers = 6;
                color = { 0, 0, 0, 255 };
                break;
            case TextureFallback::BrdfLut:
                texture->usage = requestedUsage == TextureAssetUsage::Color ? TextureAssetUsage::EnvironmentBrdfLut : requestedUsage;
                texture->shape = TextureShape::Texture2D;
                texture->arrayLayers = 1;
                color = { 255, 255, 255, 255 };
                break;
            case TextureFallback::None:
                break;
            }

            texture->pixels.resize(static_cast<size_t>(texture->arrayLayers) * color.size());
            for (uint32_t layer = 0; layer < texture->arrayLayers; ++layer)
                std::copy(color.begin(), color.end(), texture->pixels.begin() + static_cast<size_t>(layer) * color.size());
            return texture;
        }

        TextureLoadResult Successful(std::unique_ptr<TextureData> texture)
        {
            if (!texture || !IsTexturePayloadLayoutValid(*texture))
                return Failure(TextureLoadStatus::InvalidPayloadLayout, texture ? texture->path : std::string{}, "final texture payload layout is invalid");
            LOG_INFO("TextureLoader", "Texture ready path='{}' source={} size={}x{} storage={} layers={} mips={} rowBytes={} layerBytes={} totalBytes={}", texture->path, TextureSourceEncodingName(texture->sourceEncoding), texture->width, texture->height, TexturePixelStorageName(texture->pixelStorage), texture->arrayLayers, texture->mipLevels, texture->rowBytes, texture->layerBytes, texture->pixels.size());
            return { .texture = std::move(texture), .status = TextureLoadStatus::Success };
        }

        TextureLoadResult LoadTextureDescriptor(const std::string& path)
        {
            std::ifstream file(path);
            if (!file)
                return Failure(TextureLoadStatus::FileIOError, path, "could not open texture descriptor");

            nlohmann::json json;
            try
            {
                json = nlohmann::json::parse(file);
            }
            catch (const std::exception& exception)
            {
                return Failure(TextureLoadStatus::InvalidDescriptor, path, std::string("JSON parse failed: ") + exception.what());
            }

            try
            {
                const std::filesystem::path descriptorPath(path);
                const std::filesystem::path baseDir = descriptorPath.parent_path();
                const TextureAssetUsage usage = ParseUsage(json.value<std::string>("usage", "color"));
                const TextureFallback fallback = ParseFallback(json.value<std::string>("fallback", "none"));
                const auto storageRequest = Internal::ParseTextureStorageRequest(json.value<std::string>("format", "auto"));
                if (!storageRequest)
                    return Failure(TextureLoadStatus::InvalidDescriptor, path, "format must be auto, rgba16f or rgba32f");
                if (json.value("generateMips", false) || json.value("mipLevels", 1u) != 1u)
                    return Failure(TextureLoadStatus::InvalidDescriptor, path, "mip generation is not implemented; use generateMips=false and mipLevels=1");

                const std::string projectionName = Lower(json.value<std::string>("projection", "none"));
                if (projectionName != "none" && projectionName != "equirectangular")
                    return Failure(TextureLoadStatus::InvalidDescriptor, path, "projection must be none or equirectangular");
                const bool equirectangular = projectionName == "equirectangular";
                if (equirectangular && json.contains("cubeFaces"))
                    return Failure(TextureLoadStatus::InvalidDescriptor, path, "projection=equirectangular requires a single source, not cubeFaces");
                if (!equirectangular && json.contains("outputFaceSize"))
                    return Failure(TextureLoadStatus::InvalidDescriptor, path, "outputFaceSize is only valid with projection=equirectangular");

                auto texture = std::make_unique<TextureData>();
                texture->path = path;
                texture->usage = usage;
                texture->fallback = fallback;
                texture->generateMips = false;
                texture->mipLevels = 1;

                if (json.contains("cubeFaces"))
                {
                    static constexpr std::array<const char*, 6> FACE_ORDER{ "px", "nx", "py", "ny", "pz", "nz" };
                    std::vector<std::filesystem::path> faces;
                    const nlohmann::json& cubeFaces = json.at("cubeFaces");
                    if (cubeFaces.is_array())
                    {
                        if (cubeFaces.size() != FACE_ORDER.size())
                            return Failure(TextureLoadStatus::InvalidDescriptor, path, "cubeFaces must contain exactly 6 entries");
                        for (const nlohmann::json& face : cubeFaces)
                            faces.push_back(ResolvePath(baseDir, face.get<std::string>()));
                    }
                    else if (cubeFaces.is_object())
                    {
                        for (const char* faceName : FACE_ORDER)
                        {
                            if (!cubeFaces.contains(faceName))
                                return Failure(TextureLoadStatus::InvalidDescriptor, path, std::string("missing cube face ") + faceName);
                            faces.push_back(ResolvePath(baseDir, cubeFaces.at(faceName).get<std::string>()));
                        }
                    }
                    else
                        return Failure(TextureLoadStatus::InvalidDescriptor, path, "cubeFaces must be an array or object");

                    texture->shape = TextureShape::TextureCube;
                    texture->arrayLayers = 6;
                    texture->srgb = json.value("srgb", false);
                    for (const std::filesystem::path& face : faces)
                    {
                        Internal::TextureDecodeResult decoded = Internal::DecodeTextureImage(face, *storageRequest, texture->srgb);
                        if (!decoded.image)
                        {
                            if (Internal::TextureSourceEncodingFromPath(face) == TextureSourceEncoding::LDR && fallback != TextureFallback::None)
                                return Successful(CreateFallbackTexture(path, usage, fallback));
                            return Failure(decoded.status, face, decoded.message);
                        }
                        if (!texture->cubeFaces.empty() && texture->sourceEncoding != decoded.image->encoding)
                            return Failure(TextureLoadStatus::InvalidDescriptor, path, "Cubemap faces may not mix LDR, Radiance HDR and OpenEXR encodings");
                        if (texture->width == 0)
                        {
                            texture->width = decoded.image->width;
                            texture->height = decoded.image->height;
                            texture->sourceEncoding = decoded.image->encoding;
                            texture->pixelStorage = decoded.image->storage;
                            texture->rowBytes = decoded.image->rowBytes;
                            texture->layerBytes = decoded.image->layerBytes;
                        }
                        else if (texture->width != decoded.image->width || texture->height != decoded.image->height)
                            return Failure(TextureLoadStatus::InvalidDescriptor, path, "Cubemap face dimensions do not match");
                        else if (texture->pixelStorage != decoded.image->storage || texture->rowBytes != decoded.image->rowBytes || texture->layerBytes != decoded.image->layerBytes)
                            return Failure(TextureLoadStatus::InvalidPayloadLayout, path, "Cubemap face byte layouts do not match");

                        texture->cubeFaces.push_back(face.string());
                        texture->pixels.insert(texture->pixels.end(), decoded.image->pixels.begin(), decoded.image->pixels.end());
                    }
                    return Successful(std::move(texture));
                }

                if (json.contains("source"))
                {
                    const std::filesystem::path source = ResolvePath(baseDir, json.at("source").get<std::string>());
                    const TextureSourceEncoding sourceEncoding = Internal::TextureSourceEncodingFromPath(source);
                    if (equirectangular && sourceEncoding != TextureSourceEncoding::RadianceHDR && sourceEncoding != TextureSourceEncoding::OpenEXR)
                        return Failure(TextureLoadStatus::InvalidDescriptor, path, "projection=equirectangular requires a Radiance HDR or OpenEXR source");
                    texture->srgb = json.value("srgb", sourceEncoding == TextureSourceEncoding::LDR && usage == TextureAssetUsage::Color);
                    Internal::TextureDecodeResult decoded = Internal::DecodeTextureImage(source, *storageRequest, texture->srgb);
                    if (!decoded.image)
                    {
                        if (sourceEncoding == TextureSourceEncoding::LDR && fallback != TextureFallback::None)
                            return Successful(CreateFallbackTexture(path, usage, fallback));
                        return Failure(decoded.status, source, decoded.message);
                    }
                    texture->shape = TextureShape::Texture2D;
                    texture->arrayLayers = 1;
                    texture->width = decoded.image->width;
                    texture->height = decoded.image->height;
                    texture->sourceEncoding = decoded.image->encoding;
                    texture->pixelStorage = decoded.image->storage;
                    texture->rowBytes = decoded.image->rowBytes;
                    texture->layerBytes = decoded.image->layerBytes;
                    texture->pixels = std::move(decoded.image->pixels);

                    texture->sourcePath = source.string();
                    if (equirectangular)
                    {
                        texture->projection = TextureProjection::Equirectangular;
                        const uint32_t faceSize = json.value("outputFaceSize", 0u);
                        EnvironmentProjectionResult projected = ConvertEquirectangularToCubemap(*texture, { .outputFaceSize = faceSize });
                        if (!projected)
                        {
                            const TextureLoadStatus status = projected.status == EnvironmentProjectionStatus::InvalidFaceSize ? TextureLoadStatus::FaceSizeLimitExceeded
                                                                                                                             : projected.status == EnvironmentProjectionStatus::InvalidFloatPayload ? TextureLoadStatus::InvalidFloatPayload
                                                                                                                                                                                            : TextureLoadStatus::InvalidProjection;
                            return Failure(status, path, "equirectangular projection failed for '" + source.string() + "': " + projected.message);
                        }
                        projected.texture->path = path;
                        projected.texture->sourcePath = source.string();
                        projected.texture->projection = TextureProjection::Equirectangular;
                        return Successful(std::move(projected.texture));
                    }
                    return Successful(std::move(texture));
                }

                if (auto fallbackTexture = CreateFallbackTexture(path, usage, fallback))
                    return Successful(std::move(fallbackTexture));
                return Failure(TextureLoadStatus::InvalidDescriptor, path, "descriptor must contain source, cubeFaces or a fallback");
            }
            catch (const std::exception& exception)
            {
                return Failure(TextureLoadStatus::InvalidDescriptor, path, exception.what());
            }
        }
    } // namespace

    TextureLoadResult TextureLoader::LoadWithStatus(const std::string& path)
    {
        const std::filesystem::path texturePath(path);
        if (Lower(texturePath.extension().string()) == ".texture")
            return LoadTextureDescriptor(path);

        const TextureSourceEncoding sourceEncoding = Internal::TextureSourceEncodingFromPath(texturePath);
        const bool srgb = sourceEncoding == TextureSourceEncoding::LDR;
        Internal::TextureDecodeResult decoded = Internal::DecodeTextureImage(texturePath, Internal::TextureStorageRequest::Auto, srgb);
        if (!decoded.image)
            return Failure(decoded.status, texturePath, decoded.message);

        auto texture = std::make_unique<TextureData>();
        texture->path = path;
        texture->width = decoded.image->width;
        texture->height = decoded.image->height;
        texture->channels = 4;
        texture->srgb = srgb;
        texture->mipLevels = 1;
        texture->arrayLayers = 1;
        texture->sourceEncoding = decoded.image->encoding;
        texture->pixelStorage = decoded.image->storage;
        texture->rowBytes = decoded.image->rowBytes;
        texture->layerBytes = decoded.image->layerBytes;
        texture->pixels = std::move(decoded.image->pixels);
        return Successful(std::move(texture));
    }

    std::unique_ptr<TextureData> TextureLoader::Load(const std::string& path)
    {
        TextureLoadResult result = LoadWithStatus(path);
        return std::move(result.texture);
    }
} // namespace ChikaEngine::Asset
