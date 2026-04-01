#pragma once

#include "AssetLayouts.hpp"
#include <memory>
#include <string>
namespace ChikaEngine::Asset
{
    struct ShaderTemplateLoader
    {
        static std::unique_ptr<ShaderTemplateData> Load(const std::string& path);
    };

} // namespace ChikaEngine::Asset