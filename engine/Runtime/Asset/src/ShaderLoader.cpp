#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/ShaderLoader.hpp"

#include <memory>
#include <string>
#include <fstream>
namespace ChikaEngine::Asset
{

    std::unique_ptr<ShaderData> ShaderLoader::Load(const std::string& path)
    {
        auto data = std::make_unique<ShaderData>();
        data->path = path;

        std::ifstream file(path, std::ios::binary);
        data->spirv = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});

        return data;
    }
} // namespace ChikaEngine::Asset