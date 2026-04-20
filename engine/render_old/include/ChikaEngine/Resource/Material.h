/*!
 * @file Material.h
 * @brief 材质数据定义 - 纯数据结构，不包含渲染逻辑
 * @date 2026-03-07
 *
 * 设计理念：
 * - Material 只是数据容器，不包含任何 RHI 相关的逻辑
 * - 所有参数通过标准库容器存储，易于序列化
 * - ShaderHandle、TextureHandle 都使用 THandle 确保类型安全
 */
#pragma once

#include "ChikaEngine/Resource/ResourceHandles.h"
#include <array>
#include <string>
#include <unordered_map>

namespace ChikaEngine::Render
{
    /// ========== 着色器句柄定义 ==========
    using ShaderHandle = Core::THandle<struct ShaderTag>;

    /// ========== 材质数据结构 ==========
    /// 完全数据驱动，包含所有材质参数，但不包含 RHI 资源
    struct Material
    {
        /// 着色器程序
        ShaderHandle shaderHandle;

        /// 纹理参数（参数名 -> 纹理句柄）
        std::unordered_map<std::string, TextureHandle> textures;

        /// 立方体贴图（参数名 -> 立方体纹理句柄）
        std::unordered_map<std::string, Core::THandle<struct TextureCubeTag>> cubemaps;

        /// 浮点数常量（参数名 -> 值）
        std::unordered_map<std::string, float> uniformFloats;

        /// 3D 向量常量（参数名 -> 值）
        std::unordered_map<std::string, std::array<float, 3>> uniformVec3s;

        /// 4D 向量常量（参数名 -> 值）
        std::unordered_map<std::string, std::array<float, 4>> uniformVec4s;

        /// 4x4 矩阵常量（参数名 -> 值）
        std::unordered_map<std::string, std::array<float, 16>> uniformMat4s;

        /// 渲染队列（排序）
        uint32_t renderQueue = 2000;

        /// 着色器变体（可选）
        std::string shaderVariant;
    };

    /// ========== 材质句柄定义 ==========
    /// 使用 THandle 替代原始 uint32_t，提供类型安全和版本检查
    using MaterialHandle = Core::THandle<struct MaterialTag>;

} // namespace ChikaEngine::Render
