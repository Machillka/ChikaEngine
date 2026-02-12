#include "ChikaEngine/RHI/OpenGL/GLRenderDevice.h"
#include "ChikaEngine/Resource/MaterialPool.h"
#include "ChikaEngine/Resource/MeshPool.h"
#include "ChikaEngine/Resource/ShaderPool.h"
#include "ChikaEngine/Resource/TexturePool.h"
#include "ChikaEngine/render_device.h"

namespace ChikaEngine::Render
{
    GLRenderDevice::GLRenderDevice(IRHIDevice* device) : _glRHIDevice(device) {}
    GLRenderDevice::~GLRenderDevice() {}
    void GLRenderDevice::Init()
    {
        MeshPool::Init(_glRHIDevice);
        ShaderPool::Init(_glRHIDevice);
        MaterialPool::Init(_glRHIDevice);
        TexturePool::Init(_glRHIDevice);
    }

    void GLRenderDevice::BeginFrame()
    {
        _glRHIDevice->BeginFrame();
    }

    void GLRenderDevice::EndFrame()
    {
        _glRHIDevice->EndFrame();
    }

    void GLRenderDevice::DrawObject(const RenderObject& obj, const CameraData& camera)
    {
        // LOG_INFO("GLRenderDevice", "Drawing object mesh={} material={}", obj.mesh, obj.material);
        const RHIMesh& mesh = MeshPool::Get(obj.mesh);
        RHIMaterial& mat = MaterialPool::Get(obj.material);
        Math::Mat4 mvp = camera.projectionMatrix * camera.viewMatrix * obj.modelMat;
        mat.uniformMat4s["u_MVP"] = mvp;
        mat.uniformMat4s["u_Model"] = obj.modelMat;
        mat.uniformVec3s["u_CameraPos"] = {camera.position.x, camera.position.y, camera.position.z};
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