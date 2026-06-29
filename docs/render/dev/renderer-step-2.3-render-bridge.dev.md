# Renderer Step 2.3：Gameplay Render Bridge

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderSubsystem.h/.cpp`, `SceneEvents.hpp`, `GameObject.cpp`, `MeshRenderer.h`

## Goal

将 Gameplay 生命周期增量同步到 RenderWorld，停止每帧遍历所有 GameObject 并重新生成 DrawCommand。

## Implementation

- `RenderSubsystem` 订阅 Component Added/Removed/Activation 和 GameObject Destroyed 事件。
- Bridge 只维护已注册 `MeshRenderer`，不扫描无渲染组件的 GameObject。
- Mesh/Material 仅在组件 Asset Dirty、引用变化或资产热重载后重新 Resolve/Upload。
- Transform 与动画数据仅更新对应 Render Proxy；RenderWorld 会拒绝无变化更新。
- 删除、禁用或层级失活会移除对应 Render Proxy。
- `MeshRenderer` 不再持有骨骼 RHI Buffer；GPU 动态 Buffer 生命周期迁移到 RenderPipeline。

## Verification

- 搜索 Framework 不再存在 `DrawCommand`、`SubmitDrawCommands`、`SubmitLight` 或 MeshRenderer Bone UBO API。

## Remaining Boundary

Transform 字段当前公开，无法主动发送 Dirty Event；Bridge 每帧只比较已注册 Render Proxy。后续可通过 Transform Revision 进一步消除比较。
