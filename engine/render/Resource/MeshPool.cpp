#include "MeshPool.h"

#include "render/Resource/Mesh.h"
#include "render/rhi/RHIDevice.h"
#include "render/rhi/RHITypes.h"
#include "render/rhi/opengl/GLVertexArray.h"
#include "render/rhi/opengl/GLBuffer.h"
#include "render/rhi/opengl/GLHeader.h"

#include <cstdint>

namespace ChikaEngine::Render
{
    IRHIDevice* MeshPool::_device = nullptr;
    std::vector<RHIMesh> MeshPool::_meshes;

    void MeshPool::Init(IRHIDevice* device)
    {
        _device = device;
    }

    // 创建资源 返回资源句柄
    MeshHandle MeshPool::Create(const Mesh& mesh)
    {
        auto* vao = _device->CreateVertexArray();
        auto* vbo = _device->CreateVertexBuffer(mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data());
        auto* ibo = _device->CreateIndexBuffer(mesh.indices.size() * sizeof(std::uint32_t), mesh.indices.data());

        // 绑定 VAO 并将 VBO 和 IBO 关联
        auto* glVao = static_cast<GLVertexArray*>(vao);
        auto* glVbo = static_cast<GLBuffer*>(vbo);
        auto* glIbo = static_cast<GLBuffer*>(ibo);

        glBindVertexArray(glVao->Handle());
        
        // 绑定 VBO 并设置顶点属性指针
        glBindBuffer(GL_ARRAY_BUFFER, glVbo->Handle());
        // layout 0: position (vec3), layout 1: normal (vec3), layout 2: uv (vec2)
        // 每个顶点大小：3+3+2 = 8 floats = 32 bytes
        constexpr GLsizei stride = sizeof(float) * 8;
        
        glEnableVertexAttribArray(0); // position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        
        glEnableVertexAttribArray(1); // normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));
        
        glEnableVertexAttribArray(2); // uv
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));
        
        // 绑定 IBO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glIbo->Handle());
        
        glBindVertexArray(0);

        RHIMesh rhiMesh{};
        rhiMesh.vao = vao;
        rhiMesh.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
        rhiMesh.indexType = IndexType::Uint32;
        _meshes.push_back(rhiMesh);
        return static_cast<MeshHandle>(_meshes.size() - 1);
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