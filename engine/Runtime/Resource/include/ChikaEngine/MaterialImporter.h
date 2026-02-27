#pragma once

#include "ChikaEngine/Resource/Material.h"
#include "ChikaEngine/ResourceSystem.h"
#include "ChikaEngine/TextureCubeImporter.h"
#include "ChikaEngine/debug/log_macros.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ChikaEngine::Resource::Importer
{

    struct MaterialImporter
    {
        static Render::Material Load(const std::string& path, ResourceSystem& rs)
        {
            using json = nlohmann::json;

            std::ifstream ifs(path);

            if (!ifs)
            {
                LOG_ERROR("Material Importer", "Error{}", path);
                throw std::runtime_error("Material file not found: " + path);
            }

            json j;
            ifs >> j;
            Render::Material material;

            ShaderSourceDesc sd;
            sd.vertexPath = j["shader"]["vertex"];
            sd.fragmentPath = j["shader"]["fragment"];
            material.shaderHandle = rs.LoadShader(sd);

            for (auto& [name, texPath] : j["textures"].items())
            {
                material.textures[name] = rs.LoadTexture(texPath.get<std::string>());
            }

            for (auto& [name, val] : j["uniforms"].items())
            {
                {
                    if (val.is_number_float())
                    {
                        material.uniformFloats[name] = val.get<float>();
                    }
                    else if (val.is_array() && val.size() == 3)
                    {
                        material.uniformVec3s[name] = {val[0].get<float>(), val[1].get<float>(), val[2].get<float>()};
                    }
                    else if (val.is_array() && val.size() == 4)
                    {
                        material.uniformVec4s[name] = {val[0].get<float>(), val[1].get<float>(), val[2].get<float>(), val[3].get<float>()};
                    }
                }
            }

            if (j.contains("cubemaps"))
            {
                for (auto& [name, pathsJson] : j["cubemaps"].items())
                {
                    if (pathsJson.is_array() && pathsJson.size() == 6)
                    {
                        std::array<std::string, 6> faces;
                        for (int i = 0; i < 6; ++i)
                        {
                            faces[i] = pathsJson[i].get<std::string>();
                        }

                        TextureCubeSourceDesc desc;
                        desc.facePaths = faces;
                        desc.sRGB = true;

                        material.cubemaps[name] = rs.LoadTextureCube(desc);
                    }
                    else
                    {
                        LOG_WARN("MaterialImporter", "Cubemap '{}' in {} must have exactly 6 face paths.", name, path);
                    }
                }
            }

            return material;
        }
    };
} // namespace ChikaEngine::Resource::Importer
