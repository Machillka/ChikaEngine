#pragma once

#include "ChikaEngine/RHIDesc.hpp"
#include "RenderGraphHandle.hpp"
#include <string>
namespace ChikaEngine::Render
{
    struct RGTextureUsage
    {
        RGTextureHandle handle;
        ResourceState state = ResourceState::Undefined;
        TextureSubresourceRange range;
        LoadOp loadOp = LoadOp::DontCare;
        StoreOp storeOp = StoreOp::Store;
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct RGBufferUsage
    {
        RGBufferHandle handle;
        ResourceState state = ResourceState::Undefined;
        BufferRange range;
    };

    /**
     * @brief 保存 RenderGraph Texture 的逻辑身份、导入状态和编译生命周期。
     */
    struct RGTextureResource
    {
        std::string name;
        TextureDesc desc;
        TextureHandle physicalHandle;
        bool isImported = false;
        ResourceState initialState = ResourceState::Undefined;
        ResourceState finalState = ResourceState::Undefined;
        uint32_t firstUsePassIdx = UINT32_MAX;
        uint32_t lastUsePassIdx = UINT32_MAX;
        uint32_t refCount = 0;
    };

    /**
     * @brief 保存 RenderGraph Buffer 的逻辑身份、导入状态和编译生命周期。
     */
    struct RGBufferResource
    {
        std::string name;
        BufferDesc desc;
        BufferHandle physicalHandle;
        bool isImported = false;
        ResourceState initialState = ResourceState::Undefined;
        ResourceState finalState = ResourceState::Undefined;
        uint32_t firstUsePassIdx = UINT32_MAX;
        uint32_t lastUsePassIdx = UINT32_MAX;
        uint32_t refCount = 0;
    };
} // namespace ChikaEngine::Render
