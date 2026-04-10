#include "ChikaEngine/MeshLoader.hpp"
#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include <tiny_obj_loader.h>

namespace ChikaEngine::Asset
{

    std::unique_ptr<MeshData> MeshLoader::Load(const std::string& path)
    {
        LOG_INFO("Mesh Loader", "Loading {}", path);
        tinyobj::ObjReader reader;
        tinyobj::ObjReaderConfig config;
        config.triangulate = true;

        if (!reader.ParseFromFile(path, config))
        {
            LOG_ERROR("MeshLoader", "Fail to load, path: " + path);
            return nullptr;
        }

        const auto& attrib = reader.GetAttrib();
        const auto& shapes = reader.GetShapes();

        auto mesh = std::make_unique<MeshData>();
        mesh->path = path;

        std::vector<VertexData> vertices;
        std::vector<uint32_t> indices;

        for (const auto& shape : shapes)
        {
            for (const auto& idx : shape.mesh.indices)
            {
                VertexData v{};

                v.position[0] = attrib.vertices[3 * idx.vertex_index + 0];
                v.position[1] = attrib.vertices[3 * idx.vertex_index + 1];
                v.position[2] = attrib.vertices[3 * idx.vertex_index + 2];

                if (idx.normal_index >= 0)
                {
                    v.normal[0] = attrib.normals[3 * idx.normal_index + 0];
                    v.normal[1] = attrib.normals[3 * idx.normal_index + 1];
                    v.normal[2] = attrib.normals[3 * idx.normal_index + 2];
                }

                if (idx.texcoord_index >= 0)
                {
                    v.uv[0] = attrib.texcoords[2 * idx.texcoord_index + 0];
                    v.uv[1] = attrib.texcoords[2 * idx.texcoord_index + 1];
                }

                vertices.push_back(v);
                indices.push_back((uint32_t)indices.size());
            }
        }

        mesh->vertices = std::move(vertices);
        mesh->indices = std::move(indices);

        return mesh;
    }

} // namespace ChikaEngine::Asset