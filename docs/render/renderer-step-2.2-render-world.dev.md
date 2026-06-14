# Renderer Step 2.2：RenderWorld 与稳定 Proxy

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderWorld.hpp/.cpp`, `SlotMap.h`, `RenderBaselineTests.cpp`

## Goal

建立不依赖 Gameplay 类型的渲染世界与稳定对象身份。

## Unreal Reference

`RenderObjectProxy` 对应简化的 Unreal Primitive Scene Proxy；`RenderWorld` 对应当前单线程规模下的渲染 Scene。

## Implementation

- 使用带 generation 的 `SlotMap` 定义 `RenderObjectHandle`、`RenderLightHandle`、`RenderViewHandle`。
- `RenderObjectProxy` 保存 Transform、Mesh/Material Render Resource、Bounds 占位、Layer、Flags 和 CPU 骨骼矩阵。
- `RenderLightProxy` 统一描述 Directional、Point、Spot Light。
- RenderWorld 公共接口不包含 `Scene`、`GameObject` 或 `Component`。
- 无变化的 Update 不增加 World Revision。

## Verification

- 单元测试覆盖创建、无变化更新、变化更新、删除、陈旧 Handle 和 Snapshot 独立性。

## Remaining Boundary

Bounds 当前为 Phase 3 的数据占位，尚未由 Mesh Importer 计算。
