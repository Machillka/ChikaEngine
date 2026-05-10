#pragma once

#include "RHIDesc.hpp"
#include "RenderGraphHandle.hpp"
#include <string>
namespace ChikaEngine::Render
{
    struct RGResourceUsage
    {
        RGResourceUsage() {}
        RGResourceUsage(ResourceState state) : state(state) {}

        RGTextureHandle handle;
        ResourceState state;
        LoadOp loadOp = LoadOp::DontCare;
        StoreOp storeOp = StoreOp::Store;
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    };

    // 资源节点
    struct RGTextureResource
    {
        std::string name;
        TextureDesc desc;
        // 持有的 handle, 在 Execute 期间或者 Imported 的资源才有值
        TextureHandle physicalHandle;
        bool isImported = false;

        // 依赖与生命周期
        RGPassHandle producerPass;
        uint32_t firstUsePassIdx = UINT32_MAX;
        uint32_t lastUsePassIdx = UINT32_MAX;
        uint32_t refCount = 0;
    };

} // namespace ChikaEngine::Render