#include "ChikaEngine/TextureLoader.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stb_image.h>

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

        bool LoadImageRGBA(const std::filesystem::path& path, std::vector<uint8_t>& outPixels, uint32_t& outWidth, uint32_t& outHeight)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
            if (!pixels || width <= 0 || height <= 0)
            {
                LOG_ERROR("TextureLoader", "Failed to load texture {}", path.string());
                stbi_image_free(pixels);
                return false;
            }

            const size_t byteSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
            outPixels.assign(pixels, pixels + byteSize);
            outWidth = static_cast<uint32_t>(width);
            outHeight = static_cast<uint32_t>(height);
            stbi_image_free(pixels);
            return true;
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

        std::unique_ptr<TextureData> LoadTextureDescriptor(const std::string& path)
        {
            std::ifstream file(path);
            if (!file)
            {
                LOG_ERROR("TextureLoader", "Failed to open texture descriptor {}", path);
                return nullptr;
            }

            nlohmann::json json;
            try
            {
                json = nlohmann::json::parse(file);
            }
            catch (const std::exception& exception)
            {
                LOG_ERROR("TextureLoader", "Failed to parse texture descriptor {}: {}", path, exception.what());
                return nullptr;
            }

            const std::filesystem::path descriptorPath(path);
            const std::filesystem::path baseDir = descriptorPath.parent_path();
            const TextureAssetUsage usage = ParseUsage(json.value<std::string>("usage", "color"));
            const TextureFallback fallback = ParseFallback(json.value<std::string>("fallback", "none"));

            auto texture = std::make_unique<TextureData>();
            texture->path = path;
            texture->usage = usage;
            texture->fallback = fallback;
            texture->generateMips = json.value("generateMips", false);
            texture->mipLevels = std::max(1u, json.value("mipLevels", 1u));
            texture->srgb = json.value("srgb", usage == TextureAssetUsage::Color);

            if (json.contains("cubeFaces"))
            {
                static constexpr std::array<const char*, 6> FACE_ORDER{ "px", "nx", "py", "ny", "pz", "nz" };
                std::vector<std::filesystem::path> faces;
                const nlohmann::json& cubeFaces = json.at("cubeFaces");
                if (cubeFaces.is_array())
                {
                    if (cubeFaces.size() != FACE_ORDER.size())
                    {
                        LOG_ERROR("TextureLoader", "Texture descriptor {} must list exactly 6 cube faces", path);
                        return nullptr;
                    }
                    for (const nlohmann::json& face : cubeFaces)
                        faces.push_back(ResolvePath(baseDir, face.get<std::string>()));
                }
                else if (cubeFaces.is_object())
                {
                    for (const char* faceName : FACE_ORDER)
                    {
                        if (!cubeFaces.contains(faceName))
                        {
                            LOG_ERROR("TextureLoader", "Texture descriptor {} is missing cube face {}", path, faceName);
                            return nullptr;
                        }
                        faces.push_back(ResolvePath(baseDir, cubeFaces.at(faceName).get<std::string>()));
                    }
                }
                else
                {
                    LOG_ERROR("TextureLoader", "Texture descriptor {} has invalid cubeFaces field", path);
                    return nullptr;
                }

                texture->shape = TextureShape::TextureCube;
                texture->arrayLayers = 6;
                texture->srgb = json.value("srgb", false);
                for (const std::filesystem::path& face : faces)
                {
                    std::vector<uint8_t> facePixels;
                    uint32_t faceWidth = 0;
                    uint32_t faceHeight = 0;
                    if (!LoadImageRGBA(face, facePixels, faceWidth, faceHeight))
                        return CreateFallbackTexture(path, usage, fallback);
                    if (texture->width == 0)
                    {
                        texture->width = faceWidth;
                        texture->height = faceHeight;
                    }
                    else if (texture->width != faceWidth || texture->height != faceHeight)
                    {
                        LOG_ERROR("TextureLoader", "Texture descriptor {} has cube faces with mismatched dimensions", path);
                        return nullptr;
                    }
                    texture->cubeFaces.push_back(face.string());
                    texture->pixels.insert(texture->pixels.end(), facePixels.begin(), facePixels.end());
                }
                return texture;
            }

            if (json.contains("source"))
            {
                const std::filesystem::path source = ResolvePath(baseDir, json.at("source").get<std::string>());
                if (!LoadImageRGBA(source, texture->pixels, texture->width, texture->height))
                    return CreateFallbackTexture(path, usage, fallback);
                texture->shape = TextureShape::Texture2D;
                texture->arrayLayers = 1;
                return texture;
            }

            return CreateFallbackTexture(path, usage, fallback);
        }
    } // namespace

    std::unique_ptr<TextureData> TextureLoader::Load(const std::string& path)
    {
        const std::filesystem::path texturePath(path);
        if (Lower(texturePath.extension().string()) == ".texture")
            return LoadTextureDescriptor(path);

        auto tex = std::make_unique<TextureData>();
        tex->path = path;
        if (!LoadImageRGBA(texturePath, tex->pixels, tex->width, tex->height))
            return nullptr;
        tex->channels = 4;
        tex->srgb = Lower(texturePath.extension().string()) != ".hdr";
        return tex;
    }
} // namespace ChikaEngine::Asset
