#include "ChikaEngine/MeshLoader.hpp"
#include "ChikaEngine/AssetLayouts.hpp"
#include "ChikaEngine/debug/log_macros.h"
#include "ChikaEngine/GLTFHelper.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <tiny_gltf.h>

namespace ChikaEngine::Asset
{
    std::unique_ptr<MeshData> MeshLoader::Load(const std::string& path)
    {
        LOG_INFO("MeshLoader", "[Trace 1] Start loading path: {}", path);

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;
        bool ret = false;

        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        LOG_INFO("MeshLoader", "[Trace 2] Calling tinygltf Parse...");
        if (ext == ".gltf")
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
        else if (ext == ".glb")
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        else
            return nullptr;

        // 如果断言发生在这里，说明 tinygltf 自身崩溃了（模型文件有问题）
        LOG_INFO("MeshLoader", "[Trace 3] tinygltf Parse Finished. Ret: {}", ret);

        if (!ret)
            return nullptr;

        auto mesh = std::make_unique<MeshData>();
        mesh->path = path;

        if (model.meshes.empty())
            return mesh;

        auto getAccessor = [&](int index) -> const tinygltf::Accessor*
        {
            if (index < 0 || static_cast<size_t>(index) >= model.accessors.size())
                return nullptr;
            return &model.accessors[index];
        };

        const auto& gltfMesh = model.meshes[0];

        LOG_INFO("MeshLoader", "[Trace 4] Starting Primitive Loop...");
        for (const auto& primitive : gltfMesh.primitives)
        {
            auto itPos = primitive.attributes.find("POSITION");
            if (itPos == primitive.attributes.end())
                continue;

            const auto* posAcc = getAccessor(itPos->second);
            if (!posAcc)
                continue;

            size_t vertexCount = posAcc->count;
            int baseVertex = static_cast<int>(mesh->vertices.size());
            mesh->vertices.reserve(mesh->vertices.size() + vertexCount);

            const tinygltf::Accessor* normAcc = primitive.attributes.contains("NORMAL") ? getAccessor(primitive.attributes.at("NORMAL")) : nullptr;
            const tinygltf::Accessor* uvAcc = primitive.attributes.contains("TEXCOORD_0") ? getAccessor(primitive.attributes.at("TEXCOORD_0")) : nullptr;
            const tinygltf::Accessor* jointAcc = primitive.attributes.contains("JOINTS_0") ? getAccessor(primitive.attributes.at("JOINTS_0")) : nullptr;
            const tinygltf::Accessor* weightAcc = primitive.attributes.contains("WEIGHTS_0") ? getAccessor(primitive.attributes.at("WEIGHTS_0")) : nullptr;

            if (jointAcc && weightAcc)
                mesh->isSkinned = true;

            LOG_INFO("MeshLoader", "[Trace 5] Reading {} vertices...", vertexCount);

            for (size_t i = 0; i < vertexCount; ++i)
            {
                VertexData v{};
                // 注意：如果你的 VertexData 里有 std::vector，下面这种赋值是安全的

                if (auto* p = GetElementPtr<float>(model, *posAcc, i))
                {
                    v.position[0] = p[0];
                    v.position[1] = p[1];
                    v.position[2] = p[2];
                }

                if (normAcc)
                {
                    if (auto* n = GetElementPtr<float>(model, *normAcc, i))
                    {
                        v.normal[0] = n[0];
                        v.normal[1] = n[1];
                        v.normal[2] = n[2];
                    }
                }

                if (uvAcc)
                {
                    if (auto* uv = GetElementPtr<float>(model, *uvAcc, i))
                    {
                        v.uv[0] = uv[0];
                        v.uv[1] = uv[1];
                    }
                }

                if (jointAcc)
                {
                    for (size_t k = 0; k < 4; ++k)
                    {
                        if (jointAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        {
                            auto* j = GetElementPtr<uint16_t>(model, *jointAcc, i);
                            v.boneIndices[k] = j ? static_cast<float>(j[k]) : 0.0f;
                        }
                        else if (jointAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        {
                            auto* j = GetElementPtr<uint8_t>(model, *jointAcc, i);
                            v.boneIndices[k] = j ? static_cast<float>(j[k]) : 0.0f;
                        }
                        else if (jointAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        {
                            auto* j = GetElementPtr<uint32_t>(model, *jointAcc, i);
                            v.boneIndices[k] = j ? static_cast<float>(j[k]) : 0.0f;
                        }
                    }
                }

                if (weightAcc)
                {
                    if (auto* w = GetElementPtr<float>(model, *weightAcc, i))
                    {
                        memcpy(&v.boneWeights[0], w, 4 * sizeof(float));
                    }
                }

                mesh->vertices.push_back(v);
            }

            LOG_INFO("MeshLoader", "[Trace 6] Reading Indices...");
            const tinygltf::Accessor* indexAcc = getAccessor(primitive.indices);
            if (indexAcc)
            {
                size_t indexCount = indexAcc->count;
                mesh->indices.reserve(mesh->indices.size() + indexCount);

                for (size_t i = 0; i < indexCount; ++i)
                {
                    if (indexAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        if (auto* idx = GetElementPtr<uint16_t>(model, *indexAcc, i))
                            mesh->indices.push_back(static_cast<uint32_t>(*idx) + baseVertex);
                    }
                    else if (indexAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                    {
                        if (auto* idx = GetElementPtr<uint32_t>(model, *indexAcc, i))
                            mesh->indices.push_back(*idx + baseVertex);
                    }
                }
            }
            else
            {
                LOG_WARN("MeshLoader", "Primitive has no indices. Generating sequential indices...");
                for (size_t i = 0; i < vertexCount; ++i)
                {
                    mesh->indices.push_back(static_cast<uint32_t>(baseVertex + i));
                }
            }
        }

        LOG_INFO("MeshLoader", "[Trace 7] Reading Skeleton...");
        if (mesh->isSkinned && !model.skins.empty())
        {
            const auto& skin = model.skins[0];
            size_t jointCount = skin.joints.size();

            mesh->skeleton.joints.resize(jointCount);

            std::unordered_map<int, int> nodeToJointIdx;
            for (size_t i = 0; i < jointCount; ++i)
            {
                int nodeIdx = skin.joints[i];
                nodeToJointIdx[nodeIdx] = static_cast<int>(i);

                const auto& node = model.nodes[nodeIdx];

                std::string jointName = node.name.empty() ? ("Joint_" + std::to_string(nodeIdx)) : node.name;
                mesh->skeleton.joints[i].name = jointName;
                mesh->skeleton.jointNameMap[jointName] = static_cast<int>(i);

                // 提取节点里记录的初始 Transform (Bind Pose)
                if (node.translation.size() == 3)
                {
                    mesh->skeleton.joints[i].localBindPos = Math::Vector3(node.translation[0], node.translation[1], node.translation[2]);
                }
                else
                {
                    mesh->skeleton.joints[i].localBindPos = Math::Vector3::zero;
                }

                if (node.rotation.size() == 4)
                {
                    mesh->skeleton.joints[i].localBindRot = Math::Quaternion(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
                }
                else
                {
                    mesh->skeleton.joints[i].localBindRot = Math::Quaternion::Identity();
                }

                if (node.scale.size() == 3)
                {
                    mesh->skeleton.joints[i].localBindScale = Math::Vector3(node.scale[0], node.scale[1], node.scale[2]);
                }
                else
                {
                    mesh->skeleton.joints[i].localBindScale = Math::Vector3(1.0f, 1.0f, 1.0f);
                }
            }

            for (size_t i = 0; i < jointCount; ++i)
            {
                int nodeIdx = skin.joints[i];
                const auto& node = model.nodes[nodeIdx];

                // 检查该节点的所有子节点
                for (int childNodeIdx : node.children)
                {
                    // 如果子节点也在我们的骨骼列表里
                    auto it = nodeToJointIdx.find(childNodeIdx);
                    if (it != nodeToJointIdx.end())
                    {
                        int childJointIdx = it->second;
                        // 将子节点的 parent 指向当前节点
                        mesh->skeleton.joints[childJointIdx].parentIndex = static_cast<int>(i);
                    }
                }
            }

            const tinygltf::Accessor* ibmAccessor = getAccessor(skin.inverseBindMatrices);
            if (ibmAccessor)
            {
                for (size_t i = 0; i < jointCount; ++i)
                {
                    if (auto* ibmData = GetElementPtr<float>(model, *ibmAccessor, i))
                    {
                        Math::Mat4 ibm;
                        // NOTE: 列主序问题
                        for (int row = 0; row < 4; ++row)
                        {
                            for (int col = 0; col < 4; ++col)
                            {
                                ibm(row, col) = ibmData[col * 4 + row];
                            }
                        }

                        mesh->skeleton.joints[i].inverseBindMat = ibm;
                    }
                }
            }
        }

        LOG_INFO("MeshLoader", "[Trace 8] ALL SUCCESS! {}, {}", mesh->indices.size(), mesh->vertices.size());
        uint32_t maxIndex = 0;
        for (auto& v : mesh->vertices)
        {
            maxIndex = std::max({ maxIndex, (uint32_t)v.boneIndices[0], (uint32_t)v.boneIndices[1], (uint32_t)v.boneIndices[2], (uint32_t)v.boneIndices[3] });
        }
        LOG_INFO("Skin", "Max bone index = {}", maxIndex);
        return mesh;
    }

    // std::unique_ptr<MeshData> MeshLoader::Load(const std::string& path)
    // {
    //     LOG_INFO("Mesh Loader", "Loading {}", path);
    //     tinyobj::ObjReader reader;
    //     tinyobj::ObjReaderConfig config;
    //     config.triangulate = true;

    //     if (!reader.ParseFromFile(path, config))
    //     {
    //         LOG_ERROR("MeshLoader", "Fail to load, path: " + path);
    //         return nullptr;
    //     }

    //     const auto& attrib = reader.GetAttrib();
    //     const auto& shapes = reader.GetShapes();

    //     auto mesh = std::make_unique<MeshData>();
    //     mesh->path = path;

    //     std::vector<VertexData> vertices;
    //     std::vector<uint32_t> indices;

    //     for (const auto& shape : shapes)
    //     {
    //         for (const auto& idx : shape.mesh.indices)
    //         {
    //             VertexData v{};

    //             v.position[0] = attrib.vertices[3 * idx.vertex_index + 0];
    //             v.position[1] = attrib.vertices[3 * idx.vertex_index + 1];
    //             v.position[2] = attrib.vertices[3 * idx.vertex_index + 2];

    //             if (idx.normal_index >= 0)
    //             {
    //                 v.normal[0] = attrib.normals[3 * idx.normal_index + 0];
    //                 v.normal[1] = attrib.normals[3 * idx.normal_index + 1];
    //                 v.normal[2] = attrib.normals[3 * idx.normal_index + 2];
    //             }

    //             if (idx.texcoord_index >= 0)
    //             {
    //                 v.uv[0] = attrib.texcoords[2 * idx.texcoord_index + 0];
    //                 v.uv[1] = attrib.texcoords[2 * idx.texcoord_index + 1];
    //             }

    //             vertices.push_back(v);
    //             indices.push_back((uint32_t)indices.size());
    //         }
    //     }

    //     mesh->vertices = std::move(vertices);
    //     mesh->indices = std::move(indices);

    //     return mesh;
    // }

} // namespace ChikaEngine::Asset