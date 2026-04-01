#include "ChikaEngine/ShaderTemplateLoader.hpp"
#include "ChikaEngine/AssetLayouts.hpp"
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <fstream>

namespace ChikaEngine::Asset
{

    static std::unordered_map<std::string, ShaderParamTypes> _ShaderParamTypeMap{ { "float", ShaderParamTypes::Float }, { "vec2", ShaderParamTypes::Vec2 }, { "vec3", ShaderParamTypes::Vec3 }, { "vec4", ShaderParamTypes::Vec4 }, { "bool", ShaderParamTypes::Bool } };

    static ShaderParamTypes ParseParamType(const std::string& s)
    {
        return _ShaderParamTypeMap.at(s);
    }

    static void FlattenTemplateParams(const nlohmann::json& j, const std::string& prefix, ShaderTemplateData* tmpl)
    {
        for (auto& [key, value] : j.items())
        {
            std::string fullKey = prefix.empty() ? key : prefix + "." + key;

            if (value.is_object() && value.contains("type"))
            {
                ShaderParamDesc p;
                p.name = fullKey;
                p.type = ParseParamType(value["type"].get<std::string>());
                if (value.contains("default"))
                {
                    p.defaultValues = value["default"].get<std::vector<float>>();
                }
                tmpl->parameters[fullKey] = std::move(p);
            }
            else if (value.is_object())
            {
                // 如果没有 type 字段且是 object，说明它是纯粹的层级划分
                FlattenTemplateParams(value, fullKey, tmpl);
            }
        }
    }

    std::unique_ptr<ShaderTemplateData> ShaderTemplateLoader::Load(const std::string& path)
    {
        auto tmpl = std::make_unique<ShaderTemplateData>();
        std::ifstream f(path);
        nlohmann::json j;
        f >> j;

        tmpl->name = j.value("name", path);
        tmpl->vertexShaderPath = j["stages"]["vertex"];
        tmpl->fragmentShaderPath = j["stages"]["fragment"];

        if (j.contains("parameters"))
        {
            FlattenTemplateParams(j["parameters"], "", tmpl.get());
        }

        if (j.contains("textures"))
        {
            for (auto& [texName, desc] : j["textures"].items())
            {
                tmpl->textures[texName] = { texName, desc["slot"].get<uint32_t>() };
            }
        }
        return tmpl;
    }

} // namespace ChikaEngine::Asset