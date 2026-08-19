#include "ChikaEngine/AssetDatabase.hpp"
#include "ChikaEngine/AssetManager.hpp"
#include "ChikaEngine/MeshLoader.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int g_failures = 0;

    void Check(bool condition, const char* message)
    {
        if (condition)
            return;
        std::cerr << "FAILED: " << message << '\n';
        ++g_failures;
    }

    bool WriteText(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path, std::ios::trunc);
        file << text;
        return file.good();
    }

    bool NearlyEqual(float lhs, float rhs, float epsilon = 1.0e-5f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    void TestTriangleAndDispatch(const std::filesystem::path& root)
    {
        namespace Asset = ChikaEngine::Asset;
        const std::filesystem::path path = root / "triangle.OBJ";
        Check(WriteText(path,
                        "v 0 0 0\n"
                        "v 1 0 0\n"
                        "v 0 1 0\n"
                        "vt 0 0\n"
                        "vt 1 0\n"
                        "vt 0 1\n"
                        "vn 0 0 1\n"
                        "f 1/1/1 2/2/1 3/3/1\n"),
              "triangle OBJ fixture is written");
        Check(Asset::AssetDatabase::Classify(path) == Asset::AssetType::Mesh, "AssetDatabase classifies uppercase OBJ as Mesh");

        const std::unique_ptr<Asset::MeshData> mesh = Asset::MeshLoader::Load(path.string());
        Check(static_cast<bool>(mesh), "uppercase OBJ dispatch loads a triangle");
        if (!mesh)
            return;
        Check(mesh->vertices.size() == 3 && mesh->indices == std::vector<uint32_t>({ 0, 1, 2 }), "triangle expands to three sequential indexed vertices");
        Check(NearlyEqual(mesh->vertices[1].position[0], 1.0f) && NearlyEqual(mesh->vertices[2].uv[1], 1.0f), "triangle positions and UVs are preserved");
        Check(NearlyEqual(mesh->vertices[0].normal[2], 1.0f), "triangle source normal is preserved");
        Check(mesh->bounds.valid && NearlyEqual(mesh->bounds.center.x, 0.5f) && NearlyEqual(mesh->bounds.center.y, 0.5f) && mesh->bounds.sphereRadius > 0.0f, "triangle local bounds are finalized");
        Check(!mesh->isSkinned, "OBJ mesh is not marked as skinned");
    }

    void TestQuadTriangulationAndGeneratedNormals(const std::filesystem::path& root)
    {
        namespace Asset = ChikaEngine::Asset;
        const std::filesystem::path path = root / "quad.obj";
        Check(WriteText(path,
                        "v -1 -1 0\n"
                        "v 1 -1 0\n"
                        "v 1 1 0\n"
                        "v -1 1 0\n"
                        "f 1 2 3 4\n"),
              "quad OBJ fixture is written");

        const std::unique_ptr<Asset::MeshData> mesh = Asset::MeshLoader::Load(path.string());
        Check(static_cast<bool>(mesh), "quad OBJ without normals loads");
        if (!mesh)
            return;
        Check(mesh->vertices.size() == 6 && mesh->indices.size() == 6, "quad is triangulated into two expanded triangles");
        bool normalsValid = true;
        bool uvDefaultsValid = true;
        for (const Asset::VertexData& vertex : mesh->vertices)
        {
            const float normalLength = std::sqrt(vertex.normal[0] * vertex.normal[0] + vertex.normal[1] * vertex.normal[1] + vertex.normal[2] * vertex.normal[2]);
            normalsValid = normalsValid && NearlyEqual(normalLength, 1.0f) && vertex.normal[2] > 0.0f;
            uvDefaultsValid = uvDefaultsValid && NearlyEqual(vertex.uv[0], 0.0f) && NearlyEqual(vertex.uv[1], 0.0f);
        }
        Check(normalsValid, "missing OBJ normals are generated per triangle");
        Check(uvDefaultsValid, "missing OBJ UVs remain zero initialized");
        Check(mesh->bounds.valid && NearlyEqual(mesh->bounds.extents.x, 1.0f) && NearlyEqual(mesh->bounds.extents.y, 1.0f), "quad bounds cover the source positions");
    }

    void TestIndependentAttributeIndices(const std::filesystem::path& root)
    {
        namespace Asset = ChikaEngine::Asset;
        const std::filesystem::path path = root / "independent-indices.obj";
        Check(WriteText(path,
                        "v 0 0 0\n"
                        "v 1 0 0\n"
                        "v 0 1 0\n"
                        "vt 0 0\n"
                        "vt 1 0\n"
                        "vt 0 1\n"
                        "vn 1 0 0\n"
                        "vn 0 1 0\n"
                        "vn 0 0 1\n"
                        "f 1/3/2 2/1/3 3/2/1\n"),
              "independent-index OBJ fixture is written");

        const std::unique_ptr<Asset::MeshData> mesh = Asset::MeshLoader::Load(path.string());
        Check(static_cast<bool>(mesh), "OBJ with independent attribute indices loads");
        if (!mesh)
            return;
        Check(NearlyEqual(mesh->vertices[0].uv[1], 1.0f) && NearlyEqual(mesh->vertices[0].normal[1], 1.0f), "first corner uses its UV and normal indices");
        Check(NearlyEqual(mesh->vertices[1].uv[0], 0.0f) && NearlyEqual(mesh->vertices[1].normal[2], 1.0f), "second corner uses independent indices");
        Check(NearlyEqual(mesh->vertices[2].uv[0], 1.0f) && NearlyEqual(mesh->vertices[2].normal[0], 1.0f), "third corner uses independent indices");
    }

    void TestMultipleShapesAndNegativeIndices(const std::filesystem::path& root)
    {
        namespace Asset = ChikaEngine::Asset;
        const std::filesystem::path path = root / "multiple-shapes.obj";
        Check(WriteText(path,
                        "v 0 0 0\n"
                        "v 1 0 0\n"
                        "v 0 1 0\n"
                        "o First\n"
                        "f 1 2 3\n"
                        "o Second\n"
                        "f -3 -2 -1\n"),
              "multiple-shape OBJ fixture is written");

        const std::unique_ptr<Asset::MeshData> mesh = Asset::MeshLoader::Load(path.string());
        Check(static_cast<bool>(mesh), "OBJ with multiple shapes and negative indices loads");
        if (!mesh)
            return;
        Check(mesh->vertices.size() == 6 && mesh->indices.size() == 6, "multiple OBJ shapes are flattened into one mesh");
        Check(NearlyEqual(mesh->vertices[3].position[0], 0.0f) && NearlyEqual(mesh->vertices[4].position[0], 1.0f) && NearlyEqual(mesh->vertices[5].position[1], 1.0f), "negative OBJ indices resolve to source positions");
    }

    void TestFailures(const std::filesystem::path& root)
    {
        namespace Asset = ChikaEngine::Asset;
        const std::filesystem::path empty = root / "empty.obj";
        Check(WriteText(empty, "v 0 0 0\n"), "empty geometry OBJ fixture is written");
        Check(!Asset::MeshLoader::Load(empty.string()), "OBJ without faces is rejected");
        Check(!Asset::MeshLoader::Load((root / "missing.obj").string()), "missing OBJ is rejected");
        const std::filesystem::path unsupported = root / "mesh.fbx";
        Check(WriteText(unsupported, "fixture"), "unsupported mesh fixture is written");
        Check(!Asset::MeshLoader::Load(unsupported.string()), "unsupported mesh extension is rejected");
    }

    void TestAssetManagerGuidPath(const std::filesystem::path& root)
    {
        namespace Asset = ChikaEngine::Asset;
        const std::filesystem::path assetRoot = root / "asset-manager";
        std::error_code error;
        std::filesystem::create_directories(assetRoot, error);
        Check(!error, "AssetManager fixture directory is created");
        const std::filesystem::path path = assetRoot / "managed.obj";
        Check(WriteText(path, "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"), "AssetManager OBJ fixture is written");

        Asset::AssetManager assets;
        Check(assets.Initialize(assetRoot, false), "AssetManager initializes for OBJ fixture");
        const Asset::AssetRecord* record = assets.GetDatabase().FindByPath(path);
        Check(record && record->type == Asset::AssetType::Mesh, "AssetDatabase publishes OBJ as a Mesh record");
        if (record)
        {
            const Asset::MeshHandle handle = assets.LoadMesh(record->guid);
            Check(handle.IsValid(), "OBJ Mesh record loads by stable GUID");
            const Asset::MeshData* mesh = assets.GetMesh(handle);
            Check(mesh && mesh->vertices.size() == 3 && mesh->bounds.valid, "AssetManager exposes loaded OBJ MeshData");
        }
        assets.Shutdown();
    }
} // namespace

int main()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "chika_mesh_loader_tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    Check(!error, "mesh fixture directory is created");

    TestTriangleAndDispatch(root);
    TestQuadTriangulationAndGeneratedNormals(root);
    TestIndependentAttributeIndices(root);
    TestMultipleShapesAndNegativeIndices(root);
    TestFailures(root);
    TestAssetManagerGuidPath(root);

    std::filesystem::remove_all(root, error);
    if (g_failures == 0)
        std::cout << "Mesh loader OBJ checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
