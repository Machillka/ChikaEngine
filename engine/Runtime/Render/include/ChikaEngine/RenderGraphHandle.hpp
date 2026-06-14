#pragma once

#include "ChikaEngine/base/HandleTemplate.h"
namespace ChikaEngine::Render
{

    struct RGTextureTag
    {
    };
    struct RGPassTag
    {
    };
    struct RGBufferTag
    {
    };

    using RGTextureHandle = Core::THandle<RGTextureTag>;
    using RGBufferHandle = Core::THandle<RGBufferTag>;
    using RGPassHandle = Core::THandle<RGPassTag>;
} // namespace ChikaEngine::Render
