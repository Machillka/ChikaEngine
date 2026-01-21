#pragma once
#include "ResourceTypes.h"

#include <fstream>
#include <sstream>
#include <string>

namespace ChikaEngine::Resource::Importer
{
    struct ShaderSource
    {
        std::string vertex;
        std::string fragment;
    };

    struct ShaderImporter
    {
        static ShaderSource Load(const ShaderSourceDesc& desc)
        {
            ShaderSource src;
            src.vertex = ReadFile(desc.vertexPath);
            src.fragment = ReadFile(desc.fragmentPath);
            return src;
        }

        static std::string ReadFile(const std::string& path)
        {
            std::ifstream ifs(path);
            if (!ifs)
                throw std::runtime_error("Shader file not found: " + path);

            std::stringstream ss;
            ss << ifs.rdbuf();
            return ss.str();
        }
    };
} // namespace ChikaEngine::Resource::Importer
