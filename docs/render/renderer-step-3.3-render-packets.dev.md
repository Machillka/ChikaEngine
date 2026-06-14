# Renderer Step 3.3：Render Packet 与 Pass 分类

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderWorld.hpp`, `RenderQueue.hpp`, `RenderQueue.cpp`, `RenderPipeline.cpp`, `MaterialLoader.cpp`

## Goal

把可见 Render Proxy 转换为独立 Pass 输入，移除 Pipeline 对完整 RenderWorld 的遍历和布尔 Pass 分支。

## Implementation

- `RenderObjectFlags` 增加 `Transparent` 与 `Masked`，Bridge 从材质 Variant 同步分类标记。
- `RenderPacket` 保存对象、Pass、Pipeline、Material、Mesh、View Depth 和实例化资格。
- `RenderQueueSet` 分离 Shadow、Forward Opaque、GBuffer Opaque、Forward Transparent。
- 同一 Proxy 可以进入主视图与 Shadow 等多个 Queue；`DrawSceneGeometry(bool, bool)` 已删除。
- Deferred 在 Lighting 后增加独立透明 Pass；Forward 在 Opaque 后提交透明 Queue。

## Design Reason

该边界接近 Unreal 从 Primitive Scene Proxy 生成 Pass 相关 Mesh Draw Command 的方式：Scene 身份与具体 Pass 提交分离，增加新 Pass 时不再向通用 Draw 函数叠加布尔参数。

## Verification

- Forward、GBuffer、Shadow 分别消费自己的 Queue。
- 静态搜索确认旧 `DrawSceneGeometry` 路径已移除。

## Remaining Boundary

Masked 当前按 Opaque 路径分类，但尚未实现 Alpha Cutout Shader；Transparent 也尚未覆盖排序组、折射和 OIT。
