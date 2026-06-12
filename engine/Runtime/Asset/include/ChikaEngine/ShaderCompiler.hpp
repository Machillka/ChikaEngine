#pragma once

#include <filesystem>
#include <string>

namespace ChikaEngine::Asset
{
    class ShaderCompiler
    {
      public:
        explicit ShaderCompiler(std::string executable = "glslc") : m_executable(std::move(executable)) {}

        bool Compile(const std::filesystem::path& source, const std::filesystem::path& output) const;

        const std::string& GetExecutable() const
        {
            return m_executable;
        }

      private:
        static std::string Quote(const std::filesystem::path& path);

      private:
        std::string m_executable;
    };
} // namespace ChikaEngine::Asset
