#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/MaterialLoader.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <fstream>
namespace ChikaEngine::Asset
{
    /*!
     * @brief dfs 把 json 嵌套路径压成单字符串并且插入到 material 数据中
     *
     * @param  j      json 文件
     * @param  prefix 前缀
     * @param  mat    插入的 material
     * @author Machillka (machillka2007@gmail.com)
     * @date 2026-04-01
     */
    static void FlattenParameters(const nlohmann::json& j,
                                  const std::string& prefix,
                                  MaterialData* mat)
    {
        for (auto& [key, value] : j.items())
        {
            // 拼接当前的树路径
            std::string fullKey = prefix.empty() ? key : prefix + "." + key;

            // 处理嵌套
            if (value.is_object())
            {
                FlattenParameters(value, fullKey, mat);
            }

            // 数值
            else if (value.is_number())
            {
                mat->floatParams[fullKey] = value.get<float>();
            }
            else if (value.is_array())
            {
                mat->vectorParams[fullKey] = value.get<std::vector<float>>();
            }
        }
    }

    std::unique_ptr<MaterialData> MaterialLoader::Load(const std::string& path)
    {

        auto mat = std::make_unique<MaterialData>();

        std::ifstream f(path);
        nlohmann::json j;
        f >> j;

        mat->name = j.value("name", path);
        mat->shaderTemplatePath = j["shader"]["template"];

        // 拍扁 Parameters
        if (j.contains("parameters"))
        {
            FlattenParameters(j["parameters"], "", mat.get());
        }

        if (j.contains("textures"))
        {
            for (auto& [name, texPath] : j["textures"].items())
            {
                mat->textureParams[name] = texPath.get<std::string>();
            }
        }

        return mat;
    }

} // namespace ChikaEngine::Asset