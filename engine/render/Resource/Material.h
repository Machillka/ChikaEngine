#pragma once

#include "render/Resource/Texture.h"

#include <array>
#include <cstdint>

namespace ChikaEngine::Render
{
    // 从 Shader Pool 中获取 shader
    using ShaderHandle = std::uint32_t;
    // TODO: 实现 PBR 材质
    struct Material
    {
        // TODO: 创建默认材质和shader等 并且提供多种 preset
        Material() {}
        Material(ShaderHandle shaderHandle)
        {
            shaderHandle = shaderHandle;
            albedoTexture = 0;
            albedoColor = {1, 1, 1, 1};
        }

        ShaderHandle shaderHandle = 0;
        TextureHandle albedoTexture = 0; // 漫反射贴图
        std::array<float, 4> albedoColor;
    };

    using MaterialHandle = std::uint32_t;
} // namespace ChikaEngine::Render