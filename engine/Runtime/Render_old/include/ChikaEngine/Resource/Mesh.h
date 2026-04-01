/*!
 * @file Mesh.h
 * @brief 网格资源定义 - 分离数据和渲染逻辑
 * @date 2026-03-07
 *
 * 设计理念：
 * - Mesh: CPU 端数据容器，顶点和索引的集合
 * - RHIMesh: GPU 端资源包装，包含顶点数组和绘制参数
 * - 两者由 MeshPool 管理，通过 MeshHandle 查询
 * - 数据驱动设计：Mesh 可序列化，RHIMesh 由 RHI 层生成
 */
#pragma once

#include "ChikaEngine/RHI/RHIResources.h"
#include "ChikaEngine/RHI/RHITypes.h"
#include "ResourceHandles.h"
#include <array>
#include <cstdint>
#include <vector>
#include <string>

namespace ChikaEngine::Render
{
    /*!
     * @struct Vertex
     * @brief 顶点结构 - 包含位置、法线、纹理坐标
     */
    struct Vertex
    {
        std::array<float, 3> position; ///< 顶点位置 (x, y, z)
        std::array<float, 3> normal;   ///< 顶点法线 (nx, ny, nz)
        std::array<float, 2> uv;       ///< 纹理坐标 (u, v)
    };

    /*!
     * @struct Mesh
     * @brief 网格数据容器 - 纯数据，可序列化
     *
     * 特点：
     * - CPU 端数据，与 RHI 无关
     * - 包含顶点和索引数据
     * - 作为资源的主要来源，由 ResourceSystem 加载
     * - 每个 Mesh 由唯一的 MeshHandle 标识
     *
     * 使用示例：
     * @code
     *   // 通过资源系统加载
     *   MeshHandle meshHandle = ResourceSystem::LoadMesh("models/cube.obj");
     *
     *   // 从池中获取
     *   const Mesh& mesh = MeshPool::Get(meshHandle);
     *
     *   // 绘制时：
     *   const RHIMesh& rhiMesh = MeshPool::GetRHI(meshHandle);
     *   renderer->DrawMesh(rhiMesh);
     * @endcode
     */
    struct Mesh
    {
        std::string name;                   ///< 网格名称（用于识别和调试）
        std::vector<Vertex> vertices;       ///< 顶点数据
        std::vector<std::uint32_t> indices; ///< 索引数据

        // 可选：预计算数据
        std::array<float, 3> boundingBoxMin; ///< 包围盒最小值
        std::array<float, 3> boundingBoxMax; ///< 包围盒最大值
    };

    /*!
     * @struct RHIMesh
     * @brief GPU 端网格资源包装
     *
     * 特点：
     * - 由 MeshPool 创建和维护
     * - 包含 GPU 资源指针（顶点数组）
     * - 包含绘制参数（索引数量、索引类型）
     * - 与具体 RHI 实现（OpenGL/Vulkan）相关
     *
     * 使用示例：
     * @code
     *   // RHIMesh 由 MeshPool 自动创建
     *   // 用户不直接构造 RHIMesh
     *   const RHIMesh& rhiMesh = MeshPool::GetRHI(meshHandle);
     *
     *   // 用于低级渲染调用
     *   rhiDevice->DrawIndexed(rhiMesh.vao, rhiMesh.indexCount, rhiMesh.indexType);
     * @endcode
     */
    struct RHIMesh
    {
        const IRHIVertexArray* vao = nullptr;    ///< 顶点数组对象（GPU 资源）
        uint32_t indexCount = 0;                 ///< 索引数量
        IndexType indexType = IndexType::Uint32; ///< 索引数据类型
    };

    /*!
     * @typedef MeshHandle
     * @brief 网格句柄 - 版本化资源索引
     *
     * 特点：
     * - 类型安全：MeshHandle ≠ TextureHandle（编译期检查）
     * - 防止 use-after-free：包含版本号
     * - O(1) 查询性能
     * - 可以在运行时验证有效性
     *
     * 使用示例：
     * @code
     *   MeshHandle handle = ResourceSystem::LoadMesh("cube.obj");
     *
     *   // 使用 handle 查询：
     *   if (MeshPool::IsValid(handle)) {
     *       const Mesh& mesh = MeshPool::Get(handle);
     *       const RHIMesh& rhi = MeshPool::GetRHI(handle);
     *   }
     * @endcode
     */
    // 注意：MeshHandle 在 ResourceHandles.h 中定义为：
    // using MeshHandle = Core::THandle<struct MeshTag>;

} // namespace ChikaEngine::Render