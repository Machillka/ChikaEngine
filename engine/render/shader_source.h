#pragma once
#include <string>
#include <string_view>
namespace ChikaEngine::Render
{
    struct ShaderSource
    {
        // 存放路径
        std::string vertex;
        std::string fragment;

        ShaderSource() = default;
        ShaderSource(std::string_view vert, std::string_view frag) : vertex(vert), fragment(frag) {}
    };
} // namespace ChikaEngine::Render