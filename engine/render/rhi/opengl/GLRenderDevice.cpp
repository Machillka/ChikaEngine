#include "GLRenderDevice.h"

#include "debug/log_macros.h"
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

    void GLRenderDevice::DrawObject(const RenderObject& obj, const Camera& camera)
    {
        LOG_INFO("GLRenderDevice", "Drawing object mesh={} material={}", obj.mesh, obj.material);
        const RHIMesh& mesh = MeshPool::Get(obj.mesh);
        RHIMaterial& mat = MaterialPool::Get(obj.material);
        Math::Mat4 mvp = camera.ProjectionMat() * camera.ViewMat() * obj.modelMat;
        mat.uniformMat4s["u_MVP"] = mvp;
        mat.uniformMat4s["u_Model"] = obj.modelMat;
        mat.uniformVec3s["u_CameraPos"] = {camera.Position().x, camera.Position().y, camera.Position().z};
        MaterialPool::Apply(obj.material);
        _glRHIDevice->DrawIndexed(mesh);
    }

    void GLRenderDevice::Shutdown()
    {
        MeshPool::Reset();
        ShaderPool::Reset();
        MaterialPool::Reset();
    }
} // namespace ChikaEngine::Render