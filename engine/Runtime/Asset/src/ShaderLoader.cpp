#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/ShaderLoader.hpp"
#include "ChikaEngine/ShaderReflection.hpp"

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
        if (!file)
            return nullptr;
        data->spirv = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});

        // Reflection 属于导入产物；缺失时仍允许加载旧/外部 SPIR-V，但不会进入 Reflection Pipeline 路径。
        std::string reflectionError;
        data->hasReflection = ShaderReflection::Load(ShaderReflection::SidecarPath(path), data->reflection, reflectionError);
        return data;
    }
} // namespace ChikaEngine::Asset
