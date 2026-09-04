/*!
 * @file AssetLayouts.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 定义 CPU 侧 数据 layout
 * @version 0.1
 * @date 2026-03-27
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "AssetAnimation.hpp"
#include "AssetReference.hpp"
#include "ChikaEngine/math/Bounds.hpp"
#include "ChikaEngine/shader/ShaderInterface.hpp"
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace ChikaEngine::Asset
{
    // 枚举 shader 使用参数类型
    enum class ShaderParamTypes
    {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Bool,
    };

    // shader 参数 desc
    struct ShaderParamDesc
    {
        std::string name;
        ShaderParamTypes type;
        std::vector<float> defaultValues;
    };

    // Texture 参数 desc
    struct ShaderTextureDesc
    {
        std::string name;
    };

    enum class TextureAssetUsage
    {
        Color,
        Data,
        Environment,
        EnvironmentIrradiance,
        EnvironmentPrefiltered,
        EnvironmentBrdfLut,
        ReflectionProbe,
    };

    enum class TextureShape
    {
        Texture2D,
        TextureCube,
    };

    enum class TextureFallback
    {
        None,
        GrayIrradiance,
        BlackPrefilter,
        BrdfLut,
    };

    /** @brief CPU 侧像素 payload 的后端无关存储类型。 */
    enum class TexturePixelStorage : uint8_t
    {
        UNorm8,
        Float16,
        Float32,
    };

    /** @brief 记录源图像编码，用于诊断并阻止 Cubemap 混用 LDR/HDR/EXR。 */
    enum class TextureSourceEncoding : uint8_t
    {
        Generated,
        LDR,
        RadianceHDR,
        OpenEXR,
    };

    enum class TextureProjection : uint8_t
    {
        None,
        Equirectangular,
    };

    enum class TextureLoadStatus : uint8_t
    {
        Success,
        FileIOError,
        InvalidDescriptor,
        DecodeFailed,
        UnsupportedEXR,
        InvalidFloatPayload,
        InvalidPayloadLayout,
        InvalidProjection,
        FaceSizeLimitExceeded,
    };

    constexpr uint32_t TexturePixelStorageBytesPerChannel(TexturePixelStorage storage)
    {
        switch (storage)
        {
        case TexturePixelStorage::UNorm8:
            return 1;
        case TexturePixelStorage::Float16:
            return 2;
        case TexturePixelStorage::Float32:
            return 4;
        }
        return 0;
    }

    constexpr std::string_view TexturePixelStorageName(TexturePixelStorage storage)
    {
        switch (storage)
        {
        case TexturePixelStorage::UNorm8:
            return "unorm8";
        case TexturePixelStorage::Float16:
            return "float16";
        case TexturePixelStorage::Float32:
            return "float32";
        }
        return "unknown";
    }

    constexpr std::string_view TextureSourceEncodingName(TextureSourceEncoding encoding)
    {
        switch (encoding)
        {
        case TextureSourceEncoding::Generated:
            return "generated";
        case TextureSourceEncoding::LDR:
            return "ldr";
        case TextureSourceEncoding::RadianceHDR:
            return "radiance-hdr";
        case TextureSourceEncoding::OpenEXR:
            return "openexr";
        }
        return "unknown";
    }

    // Shader 模板
    struct ShaderTemplateData
    {
        std::string name;

        AssetReference vertexShader;
        AssetReference fragmentShader;

        // 参数描述
        std::unordered_map<std::string, ShaderParamDesc> parameters;
        std::unordered_map<std::string, ShaderTextureDesc> textures;
    };

    struct TextureData
    {
        std::string path;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 4;
        /** @brief 颜色纹理默认按 sRGB 采样；未来由 meta/import settings 覆盖法线等线性数据。 */
        bool srgb = true;
        bool generateMips = false;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        TextureAssetUsage usage = TextureAssetUsage::Color;
        TextureShape shape = TextureShape::Texture2D;
        TextureFallback fallback = TextureFallback::None;
        TexturePixelStorage pixelStorage = TexturePixelStorage::UNorm8;
        TextureSourceEncoding sourceEncoding = TextureSourceEncoding::Generated;
        TextureProjection projection = TextureProjection::None;
        /** @brief descriptor 的直接源文件；用于诊断、hot reload dependency 和 Cooker 展开。 */
        std::string sourcePath;
        /** @brief 紧密排列的单行与单层字节数；mip 0 的所有 layer 顺序连续。 */
        uint64_t rowBytes = 0;
        uint64_t layerBytes = 0;
        std::vector<std::string> cubeFaces;

        std::vector<uint8_t> pixels;
    };

    inline bool ComputeTexturePayloadLayout(uint32_t width, uint32_t height, uint32_t channels, uint32_t arrayLayers, TexturePixelStorage storage, uint64_t& rowBytes, uint64_t& layerBytes, uint64_t& totalBytes)
    {
        const uint64_t bytesPerChannel = TexturePixelStorageBytesPerChannel(storage);
        if (width == 0 || height == 0 || channels == 0 || arrayLayers == 0 || bytesPerChannel == 0)
            return false;

        constexpr uint64_t MAX = std::numeric_limits<uint64_t>::max();
        if (static_cast<uint64_t>(width) > MAX / channels / bytesPerChannel)
            return false;
        rowBytes = static_cast<uint64_t>(width) * channels * bytesPerChannel;
        if (static_cast<uint64_t>(height) > MAX / rowBytes)
            return false;
        layerBytes = rowBytes * height;
        if (static_cast<uint64_t>(arrayLayers) > MAX / layerBytes)
            return false;
        totalBytes = layerBytes * arrayLayers;
        return true;
    }

    inline bool IsTexturePayloadLayoutValid(const TextureData& texture)
    {
        uint64_t expectedRowBytes = 0;
        uint64_t expectedLayerBytes = 0;
        uint64_t expectedTotalBytes = 0;
        return ComputeTexturePayloadLayout(texture.width, texture.height, texture.channels, texture.arrayLayers, texture.pixelStorage, expectedRowBytes, expectedLayerBytes, expectedTotalBytes) && texture.rowBytes == expectedRowBytes && texture.layerBytes == expectedLayerBytes && expectedTotalBytes == texture.pixels.size();
    }

    struct VertexData
    {
        float position[3];
        float normal[3];
        float uv[2];

        // 每个顶点受到 四个骨骼的影响
        std::uint32_t boneIndices[4];
        float boneWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    struct MeshData
    {
        std::string path;

        std::vector<VertexData> vertices;
        std::vector<uint32_t> indices;
        Math::Bounds bounds;

        // 针对蒙皮网络的数据
        // FIXME: 上层做 MeshRender 和 SkeletonRender, 底层数据分离
        bool isSkinned = false;
        SkeletonData skeleton;
    };

    struct ShaderData
    {
        std::string path;
        std::vector<uint8_t> spirv;
        Shader::ShaderReflectionData reflection;
        bool hasReflection = false;
    };

    struct MaterialData
    {
        std::string name;

        AssetReference shaderTemplate;

        std::unordered_map<std::string, bool> variants;
        std::unordered_map<std::string, float> floatParams;
        std::unordered_map<std::string, std::vector<float>> vectorParams;
        std::unordered_map<std::string, AssetReference> textureParams;
    };

} // namespace ChikaEngine::Asset
