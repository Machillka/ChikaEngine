#include "ChikaEngine/ShaderCompiler.hpp"
#include "ChikaEngine/debug/log_macros.h"

#include <cstdlib>

namespace ChikaEngine::Asset
{
    bool ShaderCompiler::Compile(const std::filesystem::path& source, const std::filesystem::path& output) const
    {
        std::error_code error;
        std::filesystem::create_directories(output.parent_path(), error);

        const std::string command = m_executable + " " + Quote(source) + " -o " + Quote(output);
        const int result = std::system(command.c_str());
        if (result != 0)
        {
            LOG_ERROR("ShaderCompiler", "Command failed with code {}: {}", result, command);
            return false;
        }

        LOG_INFO("ShaderCompiler", "Compiled {} -> {}", source.string(), output.string());
        return true;
    }

    std::string ShaderCompiler::Quote(const std::filesystem::path& path)
    {
        std::string value = path.string();
        size_t position = 0;
        while ((position = value.find('"', position)) != std::string::npos)
        {
            value.insert(position, "\\");
            position += 2;
        }
        return '"' + value + '"';
    }
} // namespace ChikaEngine::Asset
