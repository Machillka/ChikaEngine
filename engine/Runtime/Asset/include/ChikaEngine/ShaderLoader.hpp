#pragma once

#include "AssetLayouts.hpp"
#include <memory>
namespace ChikaEngine::Asset
{
    struct ShaderLoader
    {
        static std::unique_ptr<ShaderData> Load(const std::string& path);
    };

} // namespace ChikaEngine::Asset