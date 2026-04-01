/*!
 * @file MeshLoader.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 先实现  .obj 读取
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

    struct MeshLoader
    {
        static std::unique_ptr<MeshData> Load(const std::string& path);
    };
} // namespace ChikaEngine::Asset