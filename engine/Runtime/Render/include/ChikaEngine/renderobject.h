#pragma once

#include "ChikaEngine/Resource/Material.h"
#include "ChikaEngine/Resource/Mesh.h"
#include "ChikaEngine/math/mat4.h"

namespace ChikaEngine::Render
{
    // TODO: 使用句柄+缓冲池的方式优化
    struct RenderObject
    {
        MeshHandle mesh;
        MaterialHandle material;
        Math::Mat4 modelMat = Math::Mat4::Identity();
    };
} // namespace ChikaEngine::Render