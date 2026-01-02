#include "MeshLibrary.h"

namespace ChikaEngine::Render
{

    Mesh MeshLibrary::Cube()
    {
        Mesh mesh;

        // 顶点数据（每个面两个三角形，共 36 个顶点）
        const float s = 0.5f;

        struct V
        {
            float x, y, z, nx, ny, nz, u, v;
        };
        const V vertices[] = {
            // Front face
            {-s, -s, s, 0, 0, 1, 0, 0},
            {s, -s, s, 0, 0, 1, 1, 0},
            {s, s, s, 0, 0, 1, 1, 1},
            {-s, -s, s, 0, 0, 1, 0, 0},
            {s, s, s, 0, 0, 1, 1, 1},
            {-s, s, s, 0, 0, 1, 0, 1},

            // Back face
            {s, -s, -s, 0, 0, -1, 0, 0},
            {-s, -s, -s, 0, 0, -1, 1, 0},
            {-s, s, -s, 0, 0, -1, 1, 1},
            {s, -s, -s, 0, 0, -1, 0, 0},
            {-s, s, -s, 0, 0, -1, 1, 1},
            {s, s, -s, 0, 0, -1, 0, 1},

            // Left face
            {-s, -s, -s, -1, 0, 0, 0, 0},
            {-s, -s, s, -1, 0, 0, 1, 0},
            {-s, s, s, -1, 0, 0, 1, 1},
            {-s, -s, -s, -1, 0, 0, 0, 0},
            {-s, s, s, -1, 0, 0, 1, 1},
            {-s, s, -s, -1, 0, 0, 0, 1},

            // Right face
            {s, -s, s, 1, 0, 0, 0, 0},
            {s, -s, -s, 1, 0, 0, 1, 0},
            {s, s, -s, 1, 0, 0, 1, 1},
            {s, -s, s, 1, 0, 0, 0, 0},
            {s, s, -s, 1, 0, 0, 1, 1},
            {s, s, s, 1, 0, 0, 0, 1},

            // Top face
            {-s, s, s, 0, 1, 0, 0, 0},
            {s, s, s, 0, 1, 0, 1, 0},
            {s, s, -s, 0, 1, 0, 1, 1},
            {-s, s, s, 0, 1, 0, 0, 0},
            {s, s, -s, 0, 1, 0, 1, 1},
            {-s, s, -s, 0, 1, 0, 0, 1},

            // Bottom face
            {-s, -s, -s, 0, -1, 0, 0, 0},
            {s, -s, -s, 0, -1, 0, 1, 0},
            {s, -s, s, 0, -1, 0, 1, 1},
            {-s, -s, -s, 0, -1, 0, 0, 0},
            {s, -s, s, 0, -1, 0, 1, 1},
            {-s, -s, s, 0, -1, 0, 0, 1},
        };

        for (const auto& v : vertices)
        {
            Vertex vert;
            vert.position = {v.x, v.y, v.z};
            vert.normal = {v.nx, v.ny, v.nz};
            vert.uv = {v.u, v.v};
            mesh.vertices.push_back(vert);
        }

        for (uint32_t i = 0; i < 36; ++i)
        {
            mesh.indices.push_back(i);
        }

        return mesh;
    }
} // namespace ChikaEngine::Render
