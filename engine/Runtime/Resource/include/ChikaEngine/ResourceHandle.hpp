/*!
 * @file ResourceHandle.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 定义 Resource 层使用的句柄
 * @version 0.1
 * @date 2026-03-31
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/base/HandleTemplate.h"

namespace ChikaEngine::Resource
{
    struct MeshTag
    {
    };

    struct TextureTag
    {
    };

    struct MaterialTag
    {
    };

    using MeshHandle = Core::THandle<MeshTag>;
    using TextureHandle = Core::THandle<TextureTag>;
    using MaterialHandle = Core::THandle<MaterialTag>;
} // namespace ChikaEngine::Resource