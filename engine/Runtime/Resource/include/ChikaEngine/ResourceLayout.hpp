/*!
 * @file ResourceLayout.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 资源层的 Layout 创建 —— 定义 GPU 侧的数据类型
 * @version 0.1
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "ChikaEngine/RHIResourceHandle.hpp"
#include "ChikaEngine/ResourceBinder.hpp"
#include <cstdint>

namespace ChikaEngine::Resource
{

    struct MeshGPU
    {
        Render::BufferHandle vertexBuffer;
        Render::BufferHandle indexBuffer;
        // FIXME: 实际上可能不是 32bits 或许重新进行一个枚举封装更安全
        uint32_t indexCount = 0;
        bool isUint32 = true;
    };

    struct TextureGPU
    {
        Render::TextureHandle texture;
    };

    struct MaterialGPU
    {
        Render::PipelineHandle pipeline;
        Render::BufferHandle uboBuffer;
        Render::ResourceBindingGroup bindings;
    };

} // namespace ChikaEngine::Resource