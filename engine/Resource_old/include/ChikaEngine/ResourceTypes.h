/*!
 * @file ResourceTypes.h
 * @brief 资源系统的类型和描述符定义
 * @date 2026-03-07
 *
 * 设计理念：
 * - 定义所有资源句柄和加载描述符
 * - 所有句柄都是版本化的（THandle），防止 use-after-free
 * - 支持多种资源类型：Mesh, Texture, Shader, Material, TextureCube
 *
 * 注意：
 * - 为避免循环依赖，句柄类型定义在 Render/ResourceHandles.h
 * - 此处通过 forward 声明导出，供 ResourceSystem 使用
 */
#pragma once

#include <array>
#include <string>

namespace ChikaEngine::Resource
{
    // ========== 资源加载描述符 ==========

    /*!
     * @struct MeshSourceDesc
     * @brief 网格加载描述符
     */
    struct MeshSourceDesc
    {
        std::string path; ///< 网格文件路径（相对于 Assets/）
    };

    /*!
     * @struct TextureSourceDesc
     * @brief 2D 纹理加载描述符
     */
    struct TextureSourceDesc
    {
        std::string path;  ///< 纹理文件路径（相对于 Assets/）
        bool sRGB = false; ///< 是否使用 sRGB 色彩空间
    };

    /*!
     * @struct ShaderSourceDesc
     * @brief 着色器加载描述符
     *
     * 使用示例：
     * @code
     *   ShaderSourceDesc desc;
     *   desc.vertexPath = "shaders/basic.vert";
     *   desc.fragmentPath = "shaders/basic.frag";
     *   ShaderHandle handle = ResourceSystem::Instance().LoadShader(desc);
     * @endcode
     */
    struct ShaderSourceDesc
    {
        std::string vertexPath;   ///< 顶点着色器文件路径
        std::string fragmentPath; ///< 片段着色器文件路径
    };

    /*!
     * @struct MaterialSourceDesc
     * @brief 材质加载描述符
     */
    struct MaterialSourceDesc
    {
        std::string path; ///< 材质文件路径（通常是 JSON）
    };

    /*!
     * @struct TextureCubeSourceDesc
     * @brief 立方体纹理加载描述符
     *
     * 特点：
     * - 包含 6 个面的纹理路径（Right, Left, Top, Bottom, Front, Back）
     * - 顺序必须按 Vulkan/OpenGL 标准
     * - 通常用于天空盒和环境映射
     *
     * 使用示例：
     * @code
     *   TextureCubeSourceDesc desc;
     *   desc.facePaths = {
     *       "skybox/right.png",   // +X
     *       "skybox/left.png",    // -X
     *       "skybox/top.png",     // +Y
     *       "skybox/bottom.png",  // -Y
     *       "skybox/front.png",   // +Z
     *       "skybox/back.png"     // -Z
     *   };
     *   desc.sRGB = false;
     *   TextureCubeHandle handle = ResourceSystem::Instance().LoadTextureCube(desc);
     * @endcode
     */
    struct TextureCubeSourceDesc
    {
        /// 立方体贴图的 6 个面路径
        /// 顺序：Right (0), Left (1), Top (2), Bottom (3), Front (4), Back (5)
        std::array<std::string, 6> facePaths;

        bool sRGB = true; ///< 是否使用 sRGB 色彩空间
    };

} // namespace ChikaEngine::Resource