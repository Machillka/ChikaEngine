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
        Upload,   // 资源上传
        Graphics, // 图形
        Present   // swapchain
    };

    using RGSetupCallback = std::function<void(RGPassBuilder&)>;
    using RGExecuteCallback = std::function<void(IRHICommandList*, RenderGraph*)>;

    // 渲染通道节点
    struct RGPass
    {
        std::string name;
        RGPassHandle handle;
        RGPassType type = RGPassType::Graphics; // 默认图形

        std::vector<RGResourceUsage> reads;
        std::vector<RGResourceUsage> writes;

        RGResourceUsage depthWrite;
        bool hasDepth = false;

        // 是否必须要执行 —— 如果必须要执行 可达性必须为真 ( 统计引用次数的时候直接进行赋值 1 而不是 0 然后来遍历图统计计数 )
        bool hasSideEffects = false;
        uint32_t refCount = 0;

        RGExecuteCallback executeCallback;
    };

} // namespace ChikaEngine::Render