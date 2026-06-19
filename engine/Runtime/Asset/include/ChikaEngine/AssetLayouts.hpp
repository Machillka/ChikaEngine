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
#include <string>
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

        std::vector<uint8_t> pixels;
    };

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
