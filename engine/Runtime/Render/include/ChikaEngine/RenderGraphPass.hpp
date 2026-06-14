#pragma once

#include "RenderGraphResource.hpp"
#include "RenderGraphHandle.hpp"
#include "RenderGraphResource.hpp"
#include <functional>
#include <string>
namespace ChikaEngine::Render
{
    class RGPassBuilder;
    class IRHICommandList;
    class RenderGraph;

    enum class RGPassType
    {
        Graphics,
        Compute,
        Copy,
        Present
    };

    using RGSetupCallback = std::function<void(RGPassBuilder&)>;
    using RGExecuteCallback = std::function<void(IRHICommandList*, RenderGraph*)>;

    // 渲染通道节点
    struct RGPass
    {
        std::string name;
        RGPassHandle handle;
        RGPassType type = RGPassType::Graphics; // 默认图形

        std::vector<RGTextureUsage> textureReads;
        std::vector<RGTextureUsage> textureWrites;
        std::vector<RGTextureUsage> colorWrites;
        std::vector<RGBufferUsage> bufferReads;
        std::vector<RGBufferUsage> bufferWrites;
        RGTextureUsage depthWrite;
        bool hasDepth = false;
        std::vector<RGPassHandle> dependencies;

        // 是否必须要执行 —— 如果必须要执行 可达性必须为真 ( 统计引用次数的时候直接进行赋值 1 而不是 0 然后来遍历图统计计数 )
        bool hasSideEffects = false;
        bool isCulled = false;
        uint32_t refCount = 0;

        RGExecuteCallback executeCallback;
        double lastCpuTimeMs = 0.0;
    };

} // namespace ChikaEngine::Render
