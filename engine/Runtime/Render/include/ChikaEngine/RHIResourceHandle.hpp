#pragma once

#include <ChikaEngine/base/HandleTemplate.h>

namespace ChikaEngine::Render
{
    struct BufferTag
    {
    };
    struct TextureTag
    {
    };
    struct ShaderTag
    {
    };
    struct PipelineTag
    {
    };
    struct SamplerTag
    {
    };

    // NOTE: 和 Vulkan 一样, 例如 vertex mesh等资源, 都是作为 Buffer 传入
    using BufferHandle = Core::THandle<BufferTag>;
    using TextureHandle = Core::THandle<TextureTag>;
    using ShaderHandle = Core::THandle<ShaderTag>;
    using PipelineHandle = Core::THandle<PipelineTag>;
    using SamplerHandle = Core::THandle<SamplerTag>;
}; // namespace ChikaEngine::Render