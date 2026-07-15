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
#include <string>
namespace ChikaEngine::Asset
{
    struct TextureLoadResult
    {
        std::unique_ptr<TextureData> texture;
        TextureLoadStatus status = TextureLoadStatus::DecodeFailed;
        std::string message;

        explicit operator bool() const
        {
            return texture != nullptr && status == TextureLoadStatus::Success;
        }
    };

    struct TextureLoader
    {
        static TextureLoadResult LoadWithStatus(const std::string& path);
        static std::unique_ptr<TextureData> Load(const std::string& path);
    };
} // namespace ChikaEngine::Asset
