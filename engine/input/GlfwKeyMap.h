#pragma once
#include "InputCodes.h"

#include <unordered_map>

namespace ChikaEngine::Input
{
    const std::unordered_map<KeyCode, int>& GetKeyCodeToGlfwMap();
} // namespace ChikaEngine::Input
