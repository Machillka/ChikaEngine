/*!
 * @file MeshPool.h
 * @brief 网格资源池 - 管理 CPU 端网格数据和 GPU 端顶点资源
 * @date 2026-03-07
 *
 * 设计理念：
 * - 管理 Mesh（CPU 端数据）和 RHIMesh（GPU 端资源）的对应关系
 * - 通过 MeshHandle 进行 O(1) 查询
 * - 将数据（Mesh）和渲染逻辑（RHIMesh）分离
 *
 * 架构：
 *   ResourceSystem → 从磁盘加载 Mesh 数据（.obj, .gltf 等）
 *            ↓
 *   MeshPool::Create() → 上传顶点/索引到 GPU，创建 VAO
 *            ↓
 *   MeshHandle ← 返回版本化句柄
 *            ↓
 *   RenderDevice → 通过 MeshPool::Get() 查询 RHIMesh 并绘制
 */
#pragma once

#include "ChikaEngine/RHI/RHIDevice.h"
#include "ChikaEngine/Resource/Mesh.h"
#include "ChikaEngine/Resource/ResourceHandles.h"

#include <vector>

namespace ChikaEngine::Render
{
    /*!
     * @class MeshPool
     * @brief 网格资源池 - 管理所有网格的 CPU 和 GPU 资源
     *
     * 职责：
     * - 存储所有 Mesh（CPU 端顶点和索引数据）
     * - 存储所有 RHIMesh（GPU 端顶点数组对象）
     * - 提供基于 MeshHandle 的 O(1) 查询
     * - 上传顶点数据到 GPU 内存
     * - 管理顶点数组对象（VAO）的生命周期
     *
     * 使用流程：
     * @code
     *   // 1. 初始化池
     *   MeshPool::Init(rhiDevice);
     *
     *   // 2. 创建网格（通常由 ResourceSystem 调用）
     *   Mesh mesh;
     *   mesh.name = "cube";
     *   mesh.vertices = { ... };  // 顶点数组
     *   mesh.indices = { ... };   // 索引数组
     *   // 设置包围盒信息 ...
     *
     *   MeshHandle handle = MeshPool::Create(mesh);
     *
     *   // 3. 查询和使用
     *   const Mesh& cpuData = MeshPool::GetData(handle);        // CPU 端数据
     *   const RHIMesh& rhi = MeshPool::GetRHI(handle);         // GPU 端资源
     *
     *   // 4. 渲染
     *   renderer->DrawIndexed(rhi.vao, rhi.indexCount, rhi.indexType);
     *
     *   // 5. 清理
     *   MeshPool::Reset();
     * @endcode
     *
     * 内存管理：
     * - CPU 端数据存储在 std::vector<Mesh> 中
     * - GPU 端资源（VAO、VBO、IBO）由 RHI 层创建
     * - MeshPool 通过 IRHIDevice::CreateVertexArray() 创建 VAO
     * - THandle 包含版本号，防止 use-after-free
     *
     * 线程安全性：
     * - 当前不是线程安全的
     * - 所有操作应在主线程中执行
     * - GPU 上传应该与渲染线程同步
     */
    class MeshPool
    {
      public:
        /*!
         * @brief 初始化网格池
         * @param device RHI 设备指针（用于创建 VAO）
         */
        static void Init(IRHIDevice* device);

        /*!
         * @brief 创建网格并返回句柄
         * @param mesh CPU 端网格数据（顶点和索引）
         * @return MeshHandle 版本化网格句柄
         *
         * 操作流程：
         * 1. 将顶点和索引数据上传到 GPU
         * 2. 创建顶点数组对象（VAO）
         * 3. 存储 Mesh（CPU 端数据）
         * 4. 存储 RHIMesh（GPU 端资源）
         * 5. 返回 MeshHandle
         *
         * @throws std::runtime_error 如果顶点或索引数据为空
         */
        static MeshHandle Create(const Mesh& mesh);

        /*!
         * @brief 获取 CPU 端网格数据
         * @param handle 网格句柄
         * @return 网格数据（常引用）
         */
        static const Mesh& GetData(MeshHandle handle);

        /*!
         * @brief 获取 GPU 端网格资源
         * @param handle 网格句柄
         * @return GPU 网格包装（常引用）
         *
         * 返回值包含：
         * - vao: 顶点数组对象
         * - indexCount: 索引数量
         * - indexType: 索引数据类型（Uint16/Uint32）
         */
        static const RHIMesh& GetRHI(MeshHandle handle);

        /*!
         * @brief 重置池（清理所有资源）
         */
        static void Reset();

      private:
        static IRHIDevice* _device;             ///< RHI 设备指针
        static std::vector<Mesh> _meshes;       ///< CPU 端网格数据
        static std::vector<RHIMesh> _rhiMeshes; ///< GPU 端资源（VAO）
    };

} // namespace ChikaEngine::Render