#include "GLRenderDevice.h"

#include "render/Resource/MaterialPool.h"
#include "render/Resource/MeshPool.h"
#include "render/Resource/ShaderPool.h"

namespace ChikaEngine::Render
{
    GLRenderDevice::GLRenderDevice(IRHIDevice* device) : _glRHIDevice(device) {}
    GLRenderDevice::~GLRenderDevice() {}
    void GLRenderDevice::Init()
    {
        MeshPool::Init(_glRHIDevice);
        ShaderPool::Init(_glRHIDevice);
        MaterialPool::Init(_glRHIDevice);
    }

    void GLRenderDevice::BeginFrame()
    {
        _glRHIDevice->BeginFrame();
    }

    void GLRenderDevice::EndFrame()
    {
        _glRHIDevice->EndFrame();
    }

    void GLRenderDevice::DrawObject(const RenderObject& obj, const Math::Mat4& view, const Math::Mat4& proj)
    {
        const RHIMesh& mesh = MeshPool::Get(obj.mesh);
        const RHIMaterial& mat = MaterialPool::Get(obj.material);
        Math::Mat4 mvp = proj * view * obj.modelMat;
        DrawIndexedCommand cmd;
        cmd.vao = mesh.vao;
        cmd.pipe = mat.pipeline;
        cmd.tex = mat.texture;
        cmd.indexCount = mesh.indexCount;
        cmd.indexType = mesh.indexType;
        cmd.mvpMatrix = mvp.m.data();  // 传递 MVP 矩阵
        _glRHIDevice->DrawIndexed(cmd);
    }

    void GLRenderDevice::Shutdown()
    {
        MeshPool::Reset();
        ShaderPool::Reset();
        MaterialPool::Reset();
    }
} // namespace ChikaEngine::Render