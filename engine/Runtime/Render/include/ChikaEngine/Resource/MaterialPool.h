/*!
 * @file MaterialPool.h
 * @brief 材质资源池 - 管理 CPU 端材质数据和 GPU 端管线资源
 * @date 2026-03-07
 *
 * 设计理念：
 * - 管理 Material（CPU 端数据）和 RHIMaterial（GPU 端资源）的对应关系
 * - 通过 MaterialHandle 进行 O(1) 查询
 * - 将数据（Material）和渲染逻辑（RHIMaterial）分离
 * - 支持材质变体（Shader 变量）
 *
 * 架构：
 *   ResourceSystem → 从磁盘加载 Material 数据
 *            ↓
 *   MaterialPool::Create() → 创建 GPU 资源（RHIMaterial）
 *            ↓
 *   MaterialHandle ← 返回版本化句柄
 *            ↓
 *   RenderDevice → 通过 MaterialPool::Get() 查询和应用材质
 */
#pragma once

#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/RHI/RHIResources.h"
#include "ChikaEngine/Resource/Material.h"
#include "ChikaEngine/Resource/ResourceHandles.h"

#include <array>
#include <unordered_map>
#include <vector>

namespace ChikaEngine::Render
{
    /*!
     * @struct RHIMaterial
     * @brief GPU 端材质资源包装
     *
     * 特点：
     * - 包含编译后的管线（IRHIPipeline）
     * - 包含纹理和立方体贴图的 GPU 指针
     * - 包含 uniform 变量的即时值
     * - 与 Material（CPU 端数据）一一对应
     *
     * 使用示例：
     * @code
     *   // MaterialPool 自动创建 RHIMaterial
     *   MaterialHandle handle = MaterialPool::Create(cpuMaterial);
     *
     *   // 获取 GPU 资源并应用
     *   const RHIMaterial& rhi = MaterialPool::Get(handle);
     *   device->BindPipeline(rhi.pipeline);
     * @endcode
     *
     * 设计细节：
     * - 管线来自关联的着色器编译
     * - 纹理通过 TextureHandle 在 TexturePool 中查询
     * - Uniform 值由 Material 初始化，可运行时修改
     */
    struct RHIMaterial
    {
        IRHIPipeline* pipeline = nullptr;                           ///< 图形管线（编译后的着色器）
        std::unordered_map<std::string, IRHITexture2D*> textures;   ///< 2D 纹理映射
        std::unordered_map<std::string, IRHITextureCube*> cubemaps; ///< 立方体贴图映射

        // Uniform 缓冲值（用于 push constants 或 uniform buffer）
        std::unordered_map<std::string, float> uniformFloats;               ///< Float uniform 值
        std::unordered_map<std::string, std::array<float, 3>> uniformVec3s; ///< Vec3 uniform 值
        std::unordered_map<std::string, std::array<float, 4>> uniformVec4s; ///< Vec4 uniform 值
        std::unordered_map<std::string, class Math::Mat4> uniformMat4s;     ///< Mat4 uniform 值
    };

    /*!
     * @class MaterialPool
     * @brief 材质资源池 - 管理所有材质的 CPU 和 GPU 资源
     *
     * 职责：
     * - 存储所有 Material（CPU 端数据）
     * - 存储所有 RHIMaterial（GPU 端资源）
     * - 提供基于 MaterialHandle 的 O(1) 查询
     * - 创建时自动编译着色器和上传纹理
     * - 销毁时自动清理 GPU 资源
     *
     * 使用流程：
     * @code
     *   // 1. 初始化池
     *   MaterialPool::Init(rhiDevice);
     *
     *   // 2. 创建材质（通常由 ResourceSystem 调用）
     *   Material cpuData;
     *   cpuData.shader = shaderHandle;
     *   cpuData.textures["diffuse"] = textureHandle;
     *   // ... 设置其他属性 ...
     *
     *   MaterialHandle handle = MaterialPool::Create(cpuData);
     *
     *   // 3. 查询和使用
     *   const Material& data = MaterialPool::GetData(handle);      // CPU 端数据
     *   const RHIMaterial& rhi = MaterialPool::GetRHI(handle);     // GPU 端资源
     *
     *   // 4. 应用材质（用于渲染）
     *   MaterialPool::Apply(handle);  // 绑定管线、纹理等
     *
     *   // 5. 清理
     *   MaterialPool::Reset();
     * @endcode
     *
     * 线程安全性：
     * - 当前不是线程安全的
     * - 所有操作应在主线程中执行
     *
     * 内存管理：
     * - 使用 std::vector 存储，支持快速索引
     * - THandle 包含版本号，防止 use-after-free
     * - 删除操作通过版本号失效而非移除元素
     */
    class MaterialPool
    {
      public:
        /*!
         * @brief 初始化材质池
         * @param device RHI 设备指针（用于创建管线等资源）
         */
        static void Init(IRHIDevice* device);

        /*!
         * @brief 创建材质并返回句柄
         * @param material CPU 端材质数据（包含着色器、纹理等信息）
         * @return MaterialHandle 版本化材质句柄
         *
         * 操作流程：
         * 1. 从 ShaderPool 获取着色器源码
         * 2. 编译着色器为 GPU 管线
         * 3. 从 TexturePool 获取纹理 GPU 指针
         * 4. 创建 RHIMaterial 包装 GPU 资源
         * 5. 返回 MaterialHandle
         */
        static MaterialHandle Create(const Material& material);

        /*!
         * @brief 获取 CPU 端材质数据
         * @param handle 材质句柄
         * @return 材质数据（常引用）
         */
        static const Material& GetData(MaterialHandle handle);

        /*!
         * @brief 获取 GPU 端材质资源
         * @param handle 材质句柄
         * @return GPU 材质包装（非常引用，用于绑定）
         */
        static RHIMaterial& GetRHI(MaterialHandle handle);

        /*!
         * @brief 应用材质（绑定到当前渲染状态）
         * @param handle 材质句柄
         *
         * 操作：
         * - 绑定管线
         * - 绑定纹理
         * - 设置 uniform 值
         */
        static void Apply(MaterialHandle handle);

        /*!
         * @brief 重置池（清理所有资源）
         */
        static void Reset();

      private:
        static IRHIDevice* _device;                    ///< RHI 设备指针
        static std::vector<Material> _materials;       ///< CPU 端材质数据
        static std::vector<RHIMaterial> _rhiMaterials; ///< GPU 端资源
    };

} // namespace ChikaEngine::Render