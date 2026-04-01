/*!
 * @file Shader.h
 * @brief 着色器资源定义 - 分离着色器源码和编译后的 GPU 程序
 * @date 2026-03-07
 *
 * 设计理念：
 * - Shader: CPU 端数据，包含顶点和片段着色器源代码
 * - RHIPipeline: GPU 端资源，包含编译后的 Vulkan/OpenGL 管线
 * - 两者由资源系统和管线池协调管理
 * - 着色器源码可序列化，GPU 管线由 RHI 层编译生成
 */
#pragma once

#include "ResourceHandles.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace ChikaEngine::Render
{
    /*!
     * @struct Shader
     * @brief 着色器源代码容器 - 纯数据
     *
     * 特点：
     * - 存储顶点着色器和片段着色器的源代码
     * - 不包含编译后的 GPU 代码
     * - 由 ResourceSystem 从磁盘加载（.glsl, .spv 等格式）
     * - 每个 Shader 由唯一的 ShaderHandle 标识
     *
     * 使用示例：
     * @code
     *   // 通过资源系统加载
     *   ShaderHandle handle = ResourceSystem::LoadShader("shaders/basic.vert", "shaders/basic.frag");
     *
     *   // 获取源代码
     *   const Shader& shader = ShaderPool::Get(handle);
     *
     *   // 用于反射、调试、重新编译等
     * @endcode
     */
    struct Shader
    {
        std::string name;           ///< 着色器名称（用于识别和调试）
        std::string vertexSource;   ///< 顶点着色器源代码
        std::string fragmentSource; ///< 片段着色器源代码

        // 可选：其他着色器阶段
        std::string geometrySource; ///< 几何着色器源代码（可选）
        std::string computeSource;  ///< 计算着色器源代码（可选）

        // 构造函数
        Shader() = default;

        /*!
         * @brief 从顶点和片段着色器源构造
         * @param vert 顶点着色器源代码
         * @param frag 片段着色器源代码
         */
        Shader(std::string_view vert, std::string_view frag) : vertexSource(vert), fragmentSource(frag) {}
    };

    /*!
     * @typedef ShaderHandle
     * @brief 着色器句柄 - 版本化资源索引
     *
     * 特点：
     * - 类型安全：ShaderHandle ≠ MaterialHandle（编译期检查）
     * - 防止 use-after-free：包含版本号
     * - O(1) 查询性能
     *
     * 注意：ShaderHandle 在 ResourceHandles.h 中定义为：
     * using ShaderHandle = Core::THandle<struct ShaderTag>;
     *
     * 使用示例：
     * @code
     *   ShaderHandle shaderHandle = ResourceSystem::LoadShader(vertPath, fragPath);
     *
     *   // 创建材质时关联着色器
     *   Material mat;
     *   mat.shader = shaderHandle;
     *
     *   // 创建管线时编译着色器
     *   const Shader& shaderSource = ShaderPool::Get(shaderHandle);
     *   PipelineHandle pipeline = CreatePipeline(shaderSource);
     * @endcode
     */

} // namespace ChikaEngine::Render