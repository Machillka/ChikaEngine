/*!
 * @file ResourceBinder.hpp
 * @author Machillka (machillka2007@gmail.com)
 * @brief 资源绑定的前端数据说明
 * @version 0.1
 * @date 2026-03-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "RHIResourceHandle.hpp"
#include <cstdint>
#include <vector>
namespace ChikaEngine::Render
{
    // 通用 资源描述符
    // 在每个后端可以自己解析对应的数据
    // 比如 vk -> DescriptorSet
    struct ResourceBindingGroup
    {
        struct TextureBind
        {
            uint32_t slot;
            TextureHandle tex;
        };
        struct BufferBind
        {
            uint32_t slot;
            BufferHandle buf;
            uint64_t offset;
            uint64_t size;
        };

        std::vector<TextureBind> textures;
        std::vector<BufferBind> buffers;

        void BindTexture(uint32_t slot, TextureHandle t)
        {
            textures.push_back({slot, t});
        }
        void BindBuffer(uint32_t slot, BufferHandle b, uint64_t off, uint64_t sz)
        {
            buffers.push_back({slot, b, off, sz});
        }
    };

} // namespace ChikaEngine::Render