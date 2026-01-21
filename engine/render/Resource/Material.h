#pragma once

#include "render/Resource/Texture.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace ChikaEngine::Render
{
    // 从 Shader Pool 中获取 shader
    using ShaderHandle = std::uint32_t;
    // TODO: 实现 PBR 材质
    struct Material
    {
        ShaderHandle shaderHandle;
        std::unordered_map<std::string, TextureHandle> textures;
        std::unordered_map<std::string, float> uniformFloats;
        std::unordered_map<std::string, std::array<float, 3>> uniformVec3s;
        std::unordered_map<std::string, std::array<float, 4>> uniformVec4s;
    };

    using MaterialHandle = std::uint32_t;
} // namespace ChikaEngine::Render
