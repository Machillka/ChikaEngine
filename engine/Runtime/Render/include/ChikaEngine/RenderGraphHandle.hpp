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

    using RGTextureHandle = Core::THandle<RGTextureTag>;
    using RGPassHandle = Core::THandle<RGPassTag>;
} // namespace ChikaEngine::Render