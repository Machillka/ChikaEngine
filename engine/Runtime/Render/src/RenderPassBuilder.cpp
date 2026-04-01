#include <ChikaEngine/RenderPassBuilder.hpp>
#include <ChikaEngine/RenderGraph.hpp>

namespace ChikaEngine::Render
{
    RGTextureHandle RGPassBuilder::CreateTexture(const std::string& name, const TextureDesc& desc)
    {
        return m_graph->_RegisterTexture(name, desc);
    }
    void RGPassBuilder::ReadTexture(RGTextureHandle handle, ResourceState state)
    {
        RGResourceUsage usage;
        usage.handle = handle;
        usage.state = state;
        m_pass->reads.push_back(usage);
    }

    void RGPassBuilder::WriteColor(RGTextureHandle handle, LoadOp loadOp, const float clearColor[4])
    {
        RGResourceUsage usage;
        usage.handle = handle;
        usage.state = ResourceState::RenderTarget;
        usage.loadOp = loadOp;
        usage.storeOp = StoreOp::Store;

        if (clearColor)
        {
            usage.clearColor[0] = clearColor[0];
            usage.clearColor[1] = clearColor[1];
            usage.clearColor[2] = clearColor[2];
            usage.clearColor[3] = clearColor[3];
        }

        m_pass->writes.push_back(usage);
    }

    void RGPassBuilder::WriteDepth(RGTextureHandle handle, LoadOp loadOp)
    {
        RGResourceUsage usage;
        usage.handle = handle;
        usage.state = ResourceState::DepthWrite;
        usage.loadOp = loadOp;
        usage.storeOp = StoreOp::Store;
        m_pass->depthWrite = usage;
        m_pass->hasDepth = true;
    }

} // namespace ChikaEngine::Render