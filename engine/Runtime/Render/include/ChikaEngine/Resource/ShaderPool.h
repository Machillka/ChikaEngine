/*!
 * @file ShaderPool.h
 * @brief 着色器资源池 - 管理着色器源代码和编译后的 GPU 管线
 * @date 2026-03-07
 *
 * 设计理念：
 * - 管理 Shader（CPU 端源代码）和 RHIShader（GPU 端管线）的对应关系
 * - 通过 ShaderHandle 进行 O(1) 查询
 * - 将源代码（Shader）和编译后的管线（RHIShader）分离
 * - 支持着色器源代码的缓存和重新编译
 *
 * 架构：
 *   ResourceSystem → 从磁盘加载着色器源文件（.vert, .frag）
 *            ↓
 *   ShaderPool::Create() → 编译着色器为 GPU 管线
 *            ↓
 *   ShaderHandle ← 返回版本化句柄
 *            ↓
 *   MaterialPool → 使用着色器创建材质和管线
 */
#pragma once

#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/RHI/RHIResources.h"
#include "ChikaEngine/Resource/Shader.h"
#include "ChikaEngine/Resource/ResourceHandles.h"
#include <vector>

namespace ChikaEngine::Render
{
    /*!
     * @struct RHIShader
     * @brief GPU 端着色器资源包装
     *
     * 特点：
     * - 包含编译后的图形管线（IRHIPipeline）
     * - 与 Shader（CPU 端源代码）一一对应
     * - 由 ShaderPool 自动创建和编译
     * - 用于创建材质和渲染
     *
     * 使用示例：
     * @code
     *   // ShaderPool 自动编译 RHIShader
     *   ShaderHandle handle = ShaderPool::Create(shaderSource);
     *
     *   // 获取编译后的管线
     *   const RHIShader& rhi = ShaderPool::Get(handle);
     *
     *   // 用于创建材质
     *   rhiMaterial.pipeline = rhi.pipeline;
     * @endcode
     */
    struct RHIShader
    {
        IRHIPipeline* pipeline = nullptr; ///< 编译后的图形管线
    };

    /*!
     * @class ShaderPool
     * @brief 着色器资源池 - 管理所有着色器的源代码和编译后的管线
     *
     * 职责：
     * - 存储所有 Shader（CPU 端源代码）
     * - 存储所有 RHIShader（GPU 端管线）
     * - 提供基于 ShaderHandle 的 O(1) 查询
     * - 编译着色器源代码为 GPU 管线
     * - 缓存编译结果以提高性能
     * - 管理着色器对象的生命周期
     *
     * 使用流程：
     * @code
     *   // 1. 初始化池
     *   ShaderPool::Init(rhiDevice);
     *
     *   // 2. 创建着色器（通常由 ResourceSystem 调用）
     *   Shader shader;
     *   shader.name = "basic";
     *   shader.vertexSource = vertCode;
     *   shader.fragmentSource = fragCode;
     *
     *   ShaderHandle handle = ShaderPool::Create(shader);
     *
     *   // 3. 查询和使用
     *   const Shader& source = ShaderPool::GetData(handle);   // 源代码
     *   const RHIShader& rhi = ShaderPool::GetRHI(handle);   // GPU 管线
     *
     *   // 4. 创建材质时关联着色器管线
     *   Material material;
     *   material.shader = handle;
     *
     *   // 5. 清理
     *   ShaderPool::Reset();
     * @endcode
     *
     * 编译流程：
     * - 1. 验证着色器源代码不为空
     * - 2. 为顶点和片段着色器创建 shader module
     * - 3. 创建图形管线（绑定顶点输入、片段输出等）
     * - 4. 设置渲染通道和着色器绑定
     * - 5. 返回编译后的管线
     *
     * 缓存策略：
     * - 已编译的着色器保留在 _shaders 中
     * - 不支持 hot reload（需要手动清理并重新编译）
     * - 使用 Reset() 清理所有缓存
     *
     * 线程安全性：
     * - 当前不是线程安全的
     * - 所有操作应在主线程中执行
     * - GPU 编译应该与渲染线程同步
     */
    class ShaderPool
    {
      public:
        /*!
         * @brief 初始化着色器池
         * @param device RHI 设备指针（用于编译着色器）
         */
        static void Init(IRHIDevice* device);

        /*!
         * @brief 创建着色器并返回句柄
         * @param shader CPU 端着色器源代码
         * @return ShaderHandle 版本化着色器句柄
         *
         * 操作流程：
         * 1. 验证顶点和片段着色器源代码
         * 2. 编译顶点着色器为 SPIR-V（Vulkan）或二进制（OpenGL）
         * 3. 编译片段着色器
         * 4. 创建图形管线
         * 5. 存储 Shader 源代码
         * 6. 返回 ShaderHandle
         *
         * @throws std::runtime_error 如果着色器编译失败
         */
        static ShaderHandle Create(const Shader& shader);

        /*!
         * @brief 获取 CPU 端着色器源代码
         * @param handle 着色器句柄
         * @return 着色器源代码（常引用）
         */
        static const Shader& GetData(ShaderHandle handle);

        /*!
         * @brief 获取 GPU 端着色器资源
         * @param handle 着色器句柄
         * @return GPU 着色器包装（常引用）
         *
         * 返回值包含编译后的管线指针
         */
        static const RHIShader& GetRHI(ShaderHandle handle);

        /*!
         * @brief 重置池（清理所有资源）
         */
        static void Reset();

      private:
        static IRHIDevice* _device;                ///< RHI 设备指针
        static std::vector<Shader> _shaders;       ///< CPU 端着色器源代码
        static std::vector<RHIShader> _rhiShaders; ///< GPU 端管线
    };

} // namespace ChikaEngine::Render