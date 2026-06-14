# Renderer Step 3.1：Mesh Bounds

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `Bounds.hpp`, `Bounds.cpp`, `AssetLayouts.hpp`, `MeshLoader.cpp`, `ResourceLayout.hpp`, `RenderSubsystem.cpp`

## Goal

建立从 Mesh 导入数据到 RenderWorld Proxy 的统一空间范围，使可见性阶段不需要读取顶点。

## Implementation

- `Math::Bounds` 同时保存 AABB `center/extents`、外接球半径和有效标记，作为 Asset、Resource、RenderWorld 的共同契约。
- `MeshLoader` 导入时遍历顶点一次，计算 Local AABB 与真实外接球；蒙皮 Mesh 暂时保守扩张 25%。
- `ResourceManager` 将 Local Bounds 随 `MeshGPU` 上传边界一起保存。
- `RenderSubsystem` 使用 World Matrix 的绝对旋转缩放矩阵生成保守 World AABB，并处理非均匀缩放。

## Design Reason

这对应 Unreal 的 `FBoxSphereBounds` 思路：局部资产只计算一次空间范围，Scene Proxy 保存可直接用于渲染线程空间查询的 World Bounds。Gameplay 不参与 Renderer 的顶点级判断。

## Verification

- 单元测试覆盖平移、非均匀缩放和保守扩张。
- 基准场景全部可绘制 Proxy 都携带有效 World Bounds。

## Remaining Boundary

蒙皮 Bounds 仍是导入 Bounds 的保守扩张；后续可使用动画 Clip Bounds、骨骼 Bounds 或 GPU Reduction 更新动态范围。
