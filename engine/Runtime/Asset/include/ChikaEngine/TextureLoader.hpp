/*!
 * @file TextureLoader.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief  加载 texture ( image assets )
 * @version 0.1
 * @date 2026-03-27
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "AssetLayouts.hpp"
#include <memory>
namespace ChikaEngine::Asset
{

    struct TextureLoader
    {
        static std::unique_ptr<TextureData> Load(const std::string& path);
    };
} // namespace ChikaEngine::Asset