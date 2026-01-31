#include "MeshPool.h"

#include "debug/log_macros.h"
#include "render/Resource/Mesh.h"
#include "render/rhi/RHIDevice.h"
#include "render/rhi/RHITypes.h"

#include <cstdint>

namespace ChikaEngine::Render
{
    IRHIDevice* MeshPool::_device = nullptr;
    std::vector<RHIMesh> MeshPool::_meshes;

    void MeshPool::Init(IRHIDevice* device)
    {
        _device = device;
    }

    // TODO: 把各种 poll 与底层解耦
    // 创建资源 返回资源句柄
    MeshHandle MeshPool::Create(const Mesh& mesh)
    {
        auto* vao = _device->CreateVertexArray();
        auto* vbo = _device->CreateVertexBuffer(mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data());
        auto* ibo = _device->CreateIndexBuffer(mesh.indices.size() * sizeof(std::uint32_t), mesh.indices.data());

        _device->SetupMeshVertexLayout(vao, vbo, ibo);

        RHIMesh rhiMesh{};
        rhiMesh.vao = vao;
        rhiMesh.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
        rhiMesh.indexType = IndexType::Uint32;
        _meshes.push_back(rhiMesh);

        MeshHandle h = static_cast<MeshHandle>(_meshes.size() - 1);
        LOG_INFO("MeshPool", "Created mesh handle={} indexCount={} total={}", h, rhiMesh.indexCount, _meshes.size());
        return h;
    }
    void MeshPool::Reset()
    {
        _device = nullptr;
        _meshes.clear();
    }
    const RHIMesh& MeshPool::Get(MeshHandle handle)
    {
        return _meshes[handle];
    }
} // namespace ChikaEngine::Render