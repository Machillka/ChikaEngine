# Renderer Step 2.5：View Family 与 Light Proxy

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderWorld.hpp`, `RenderSubsystem.cpp`, `Renderer.cpp`, `RenderPipeline.cpp`

## Goal

将相机和光源作为 RenderWorld 输入，移除 Renderer 内部特殊提交状态。

## Implementation

- `RenderView` 保存 View、Projection、ViewProjection、位置、Layer 和 Primary 标记。
- `ViewFamily` 支持同帧多个 View，并提供 Primary View 查询。
- `RenderLightProxy` 统一 Directional、Point、Spot Light 数据。
- 编辑器 Camera 在 Bridge 提交前转换为值类型 `RenderView`；RenderPipeline 不持有 Camera 指针。
- 删除 `SubmitLight`，Scene UBO 从 Snapshot 中的 Primary View 和 Light 更新。

## Verification

- Renderer/Pipeline 不再接收 Gameplay Camera 或 Light 特殊提交。
- 当前 Forward、Deferred 与 Shadow 使用 Snapshot View/Light 完成渲染。

## Remaining Boundary

当前 Gameplay 尚无正式 Camera/Light Component，因此 Bridge 创建编辑器 Primary View 和默认方向光 Proxy；未来组件只需更新同一 RenderWorld API。
