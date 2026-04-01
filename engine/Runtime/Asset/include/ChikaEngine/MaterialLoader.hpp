#pragma once
#include "AssetLayouts.hpp"
#include <string>
#include <memory>

namespace ChikaEngine::Asset
{

    struct MaterialLoader
    {
        static std::unique_ptr<MaterialData> Load(const std::string& path);
    };
} // namespace ChikaEngine::Asset