#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace ChikaEngine::Render
{
    struct Shader
    {
        // 存放源文件
        std::string vertexSource;
        std::string fragmentSource;

        Shader() = default;
        Shader(std::string_view vert, std::string_view frag) : vertexSource(vert), fragmentSource(frag) {}
    };

    using ShaderHandle = std::uint32_t;
} // namespace ChikaEngine::Render