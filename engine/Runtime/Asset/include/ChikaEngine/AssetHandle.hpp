/*!
 * @file AssetHandle.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief handle 定义, Asset::Handle 避免重复, 所以不额外命名成 Asset__Handle
 * @version 0.1
 * @date 2026-03-27
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/base/HandleTemplate.h"
namespace ChikaEngine::Asset
{

    // Render Assets
    struct TextureTag
    {
    };
    struct MeshTag
    {
    };
    struct ShaderTag
    {
    };
    struct ShaderTemplateTag
    {
    };
    struct MaterialTag
    {
    };

    using TextureHandle = Core::THandle<TextureTag>;
    using MeshHandle = Core::THandle<MeshTag>;
    using ShaderHandle = Core::THandle<ShaderTag>;
    using ShaderTemplateHandle = Core::THandle<ShaderTemplateTag>;
    using MaterialHandle = Core::THandle<MaterialTag>;

    // Animation Assets
    struct AnimationClipTag
    {
    };
    using AnimationClipHandle = Core::THandle<AnimationClipTag>;
} // namespace ChikaEngine::Asset