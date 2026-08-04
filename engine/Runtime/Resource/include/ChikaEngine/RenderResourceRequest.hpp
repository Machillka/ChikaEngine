/*!
 * @file RenderResourceRequest.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 记录 资源提交命令所需要的句柄结构
 * @version 0.1
 * @date 2026-04-01
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "ChikaEngine/RHIDesc.hpp"
#include "ChikaEngine/RHIResourceHandle.hpp"
namespace ChikaEngine::Resource
{

    enum class TextureUploadStatus : uint8_t
    {
        Unknown,
        Ready,
        MissingAsset,
        InvalidPayload,
        DimensionLimitExceeded,
        GPUUploadFailed,
    };

    struct BufferUploadRequest
    {
        Render::BufferHandle staging;
        Render::BufferHandle dst;
        uint64_t size;
        Render::ResourceState finalState = Render::ResourceState::StorageRead;
    };

    struct TextureUploadRequest
    {
        Render::BufferHandle staging;
        Render::TextureHandle dst;
        uint32_t width;
        uint32_t height;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        uint64_t size = 0;
        uint64_t rowBytes = 0;
        uint64_t layerBytes = 0;
        Render::RHI_Format format = Render::RHI_Format::RGBA8_UNorm;
        Render::TextureDimension dimension = Render::TextureDimension::Texture2D;
    };
} // namespace ChikaEngine::Resource
