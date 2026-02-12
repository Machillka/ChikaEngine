#pragma once

#include "ChikaEngine/Resource/Mesh.h"
#include "ChikaEngine/debug/log_macros.h"
#include "tiny_obj_loader.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ChikaEngine::Resource::Importer
{
    struct MeshImporter
    {
        static ChikaEngine::Render::Mesh Load(const std::string& path)
        {
            tinyobj::attrib_t attrib;
            std::vector<tinyobj::shape_t> shapes;
            std::vector<tinyobj::material_t> materials;
            std::string warn, err;

            bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());

            if (!ok)
            {
                LOG_ERROR("Mesh Importer", "OBJ load failed: {}", warn + err);
                throw std::runtime_error("OBJ load failed: " + warn + err);
            }

            Render::Mesh mesh{};
            for (auto& shape : shapes)
            {
                for (auto& idx : shape.mesh.indices)
                {
                    Render::Vertex v{};
                    // position
                    v.position = {attrib.vertices[3 * idx.vertex_index + 0], attrib.vertices[3 * idx.vertex_index + 1], attrib.vertices[3 * idx.vertex_index + 2]};

                    // normal
                    if (idx.normal_index >= 0 && !attrib.normals.empty())
                    {
                        v.normal = {attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1], attrib.normals[3 * idx.normal_index + 2]};
                    }

                    // uv
                    if (idx.texcoord_index >= 0 && !attrib.texcoords.empty())
                    {
                        v.uv = {attrib.texcoords[2 * idx.texcoord_index + 0], attrib.texcoords[2 * idx.texcoord_index + 1]};
                    }
                    mesh.vertices.push_back(v);
                    mesh.indices.push_back((uint32_t)mesh.indices.size());
                }
            }
            return mesh;
        }
    };
} // namespace ChikaEngine::Resource::Importer