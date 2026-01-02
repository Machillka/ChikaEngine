#pragma once
#include "math/mat4.h"
#include "render/Resource/Material.h"
#include "render/Resource/Mesh.h"
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