/*!
 * @file EditorContext.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 黑板模式
 * @version 0.1
 * @date 2026-04-26
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/Renderer.hpp"
#include "ChikaEngine/base/UIDGenerator.h"
#include <string>
namespace ChikaEngine::Editor
{
    struct EditorContext
    {
        Render::Renderer* renderer = nullptr;

        Core::GameObjectID selectedGameObject;
        std::string selectedPath = "";

        bool isViewportHovered = false;
        bool isViewportFocused = false;

        bool isDirty = false;
    };
} // namespace ChikaEngine::Editor