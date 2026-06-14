#include <ChikaEngine/RenderPassBuilder.hpp>
#include <ChikaEngine/RenderGraph.hpp>

namespace ChikaEngine::Render
{
    /**
     * @brief 创建由 RenderGraph 管理生命周期的临时 Texture。
     */
    RGTextureHandle RGPassBuilder::CreateTexture(const std::string& name, const TextureDesc& desc)
    {
        return m_graph->_RegisterTexture(name, desc);
    }

    /**
     * @brief 创建由 RenderGraph 管理生命周期的临时 Buffer。
     */
    RGBufferHandle RGPassBuilder::CreateBuffer(const std::string& name, const BufferDesc& desc)
    {
        return m_graph->_RegisterBuffer(name, desc);
    }

    /**
     * @brief 声明当前 Pass 读取指定 Texture 状态。
     */
    void RGPassBuilder::ReadTexture(RGTextureHandle handle, ResourceState state, const TextureSubresourceRange& range)
    {
        RGTextureUsage usage;
        usage.handle = handle;
        usage.state = state;
        usage.range = range;
        m_pass->textureReads.push_back(usage);
    }

    /**
     * @brief 声明非 Attachment Texture 写入，例如 CopyDst 或 StorageWrite。
     */
    void RGPassBuilder::WriteTexture(RGTextureHandle handle, ResourceState state, const TextureSubresourceRange& range)
    {
        RGTextureUsage usage;
        usage.handle = handle;
        usage.state = state;
        usage.range = range;
        m_pass->textureWrites.push_back(usage);
    }

    /**
     * @brief 声明当前 Graphics Pass 的 Color Attachment。
     */
    void RGPassBuilder::WriteColor(RGTextureHandle handle, LoadOp loadOp, const float clearColor[4])
    {
        RGTextureUsage usage;
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

        m_pass->colorWrites.push_back(usage);
    }

    /**
     * @brief 声明当前 Graphics Pass 的唯一 Depth Attachment。
     */
    void RGPassBuilder::WriteDepth(RGTextureHandle handle, LoadOp loadOp)
    {
        RGTextureUsage usage;
        usage.handle = handle;
        usage.state = ResourceState::DepthWrite;
        usage.loadOp = loadOp;
        usage.storeOp = StoreOp::Store;
        m_pass->depthWrite = usage;
        m_pass->hasDepth = true;
    }

    /**
     * @brief 声明当前 Pass 读取指定 Buffer 状态。
     */
    void RGPassBuilder::ReadBuffer(RGBufferHandle handle, ResourceState state, const BufferRange& range)
    {
        m_pass->bufferReads.push_back({ handle, state, range });
    }

    /**
     * @brief 声明当前 Pass 写入指定 Buffer 状态。
     */
    void RGPassBuilder::WriteBuffer(RGBufferHandle handle, ResourceState state, const BufferRange& range)
    {
        m_pass->bufferWrites.push_back({ handle, state, range });
    }

} // namespace ChikaEngine::Render
