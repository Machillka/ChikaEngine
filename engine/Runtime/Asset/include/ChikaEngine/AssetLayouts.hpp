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
        uint32_t slot = 0;
    };

    // Shader 模板
    struct ShaderTemplateData
    {
        std::string name;

        // Shader stage 路径 Asset 层只存路径
        std::string vertexShaderPath;
        std::string fragmentShaderPath;

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

        std::vector<uint8_t> pixels;
    };

    struct VertexData
    {
        float position[3];
        float normal[3];
        float uv[2];
    };

    struct MeshData
    {
        std::string path;

        std::vector<VertexData> vertices;
        std::vector<uint32_t> indices;
    };

    struct ShaderData
    {
        std::string path;
        std::vector<uint8_t> spirv;
    };

    struct MaterialData
    {
        std::string name;

        std::string shaderTemplatePath;

        std::unordered_map<std::string, bool> variants;
        std::unordered_map<std::string, float> floatParams;
        std::unordered_map<std::string, std::vector<float>> vectorParams;
        std::unordered_map<std::string, std::string> textureParams;
    };

} // namespace ChikaEngine::Asset