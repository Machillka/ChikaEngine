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
        Render::RHI_Format format = Render::RHI_Format::RGBA8_UNorm;
    };
} // namespace ChikaEngine::Resource
