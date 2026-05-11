#pragma once

#include <cstdint>
#include <string>
namespace ChikaEngine::Platform
{

    struct WindowDesc
    {
        std::string title = "ChikaEngine";
        std::uint32_t width = 1280;
        std::uint32_t height = 720;
        bool isFullscreen = false;
    };
} // namespace ChikaEngine::Platform