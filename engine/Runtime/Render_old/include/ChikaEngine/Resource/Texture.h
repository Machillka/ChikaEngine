/*!
 * @file Texture.h
 * @brief 纹理资源定义 - 分离数据和 GPU 资源
 * @date 2026-03-07
 *
 * 设计理念：
 * - Texture: CPU 端数据容器，纹理元数据
 * - RHITexture2D/RHITextureCube: GPU 端资源包装
 * - 两者由相应池（TexturePool）管理，通过 TextureHandle 查询
 * - 数据驱动：纹理元数据可序列化，GPU 资源由 RHI 层生成
 */
#pragma once

#include <cstdint>
#include <string>
#include <array>

namespace ChikaEngine::Render
{
    /*!
     * @struct Texture
     * @brief 纹理数据容器 - 纯数据，可序列化
     *
     * 特点：
     * - CPU 端元数据
     * - 包含宽度、高度、通道数等信息
     * - 不包含实际像素数据（像素存储在 GPU 或临时缓冲区）
     * - 由 ResourceSystem 从磁盘加载
     * - 每个 Texture 由唯一的 TextureHandle 标识
     *
     * 使用示例：
     * @code
     *   // 通过资源系统加载
     *   TextureHandle handle = ResourceSystem::LoadTexture("textures/wood.png");
     *
     *   // 获取元数据
     *   const Texture& tex = TexturePool::Get(handle);
     *   printf("Texture: %dx%d, channels=%d\n", tex.width, tex.height, tex.channels);
     *
     *   // 获取 GPU 资源
     *   const RHITexture2D& rhi = TexturePool::GetRHI(handle);
     * @endcode
     */
    struct Texture
    {
        std::string name;  ///< 纹理名称（用于识别和调试）
        int width = 0;     ///< 纹理宽度（像素）
        int height = 0;    ///< 纹理高度（像素）
        int channels = 4;  ///< 颜色通道数 (1=R, 2=RG, 3=RGB, 4=RGBA)
        bool srgb = false; ///< 是否使用 sRGB 色彩空间（用于漫反射贴图）
    };

    /*!
     * @struct RHITexture2D
     * @brief 2D 纹理 GPU 资源包装
     *
     * 特点：
     * - GPU 资源指针（IRHITexture2D）
     * - 包含采样器、图像布局等信息
     * - 由 TexturePool 创建和维护
     * - 与具体 RHI 实现（OpenGL/Vulkan）相关
     */
    struct RHITexture2D
    {
        const class IRHITexture2D* texture = nullptr; ///< GPU 纹理资源
        // 可选：采样器、布局等 RHI 特定信息
    };

    /*!
     * @struct RHITextureCube
     * @brief 立方体纹理 GPU 资源包装
     *
     * 特点：
     * - 6 个面的纹理（+X, -X, +Y, -Y, +Z, -Z）
     * - 用于环境映射、天空盒等
     * - GPU 资源由 RHI 层管理
     */
    struct RHITextureCube
    {
        const class IRHITexture2D* texture = nullptr; ///< GPU 立方体纹理资源
        // 每个面的元数据可选
    };

    /*!
     * @typedef TextureHandle
     * @brief 2D 纹理句柄 - 版本化资源索引
     *
     * 特点：
     * - 类型安全：TextureHandle ≠ MeshHandle（编译期检查）
     * - 防止 use-after-free：包含版本号
     * - O(1) 查询性能
     *
     * 注意：TextureHandle 在 ResourceHandles.h 中定义为：
     * using TextureHandle = Core::THandle<struct TextureTag>;
     */

    /*!
     * @typedef TextureCubeHandle
     * @brief 立方体纹理句柄 - 版本化资源索引
     *
     * 注意：TextureCubeHandle 在 ResourceHandles.h 中定义为：
     * using TextureCubeHandle = Core::THandle<struct TextureCubeTag>;
     */

} // namespace ChikaEngine::Render