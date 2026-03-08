/*!
 * @file ResourceHandles.h
 * @brief 所有资源句柄定义 - 使用版本化 THandle 确保类型安全
 * @date 2026-03-07
 *
 * 设计理念：
 * - 所有资源使用 THandle<T> 而非原始指针或整数
 * - 每个资源类型有专属的 Tag，确保类型安全（MeshHandle ≠ TextureHandle）
 * - THandle 包含版本号，防止 use-after-free 问题
 * - O(1) 查询性能，基于索引的资源池访问
 */
#pragma once

#include "ChikaEngine/base/HandleTemplate.h"

namespace ChikaEngine::Render
{
    // ========== 资源句柄定义 ==========
    // 每个句柄有唯一的 Tag 类型，确保编译期类型检查

    /// 网格句柄
    using MeshHandle = Core::THandle<struct MeshTag>;

    /// 2D 纹理句柄
    using TextureHandle = Core::THandle<struct TextureTag>;

    /// 立方体纹理句柄
    using TextureCubeHandle = Core::THandle<struct TextureCubeTag>;

    /// 着色器句柄
    using ShaderHandle = Core::THandle<struct ShaderTag>;

    /// 材质句柄
    using MaterialHandle = Core::THandle<struct MaterialTag>;

    /// 图形管线句柄
    using PipelineHandle = Core::THandle<struct PipelineTag>;

    /// 描述符集句柄
    using BindGroupHandle = Core::THandle<struct BindGroupTag>;

    /// 渲染目标句柄
    using RenderTargetHandle = Core::THandle<struct RenderTargetTag>;

} // namespace ChikaEngine::Render