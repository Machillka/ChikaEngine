#pragma once

namespace ChikaEngine::Render
{
    // render api enum -> For choosing the rendering backend
    enum class RenderAPI
    {
        None = 0,
        OpenGL = 1,
        Vulkan = 2
    };
} // namespace ChikaEngine::Render