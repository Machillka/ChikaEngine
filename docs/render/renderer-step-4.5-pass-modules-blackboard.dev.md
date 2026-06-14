# Renderer Step 4.5：Pass Module 与 Blackboard

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderGraphBlackboard.hpp`, `RenderPipelinePasses.hpp`, `RenderPipelinePasses.cpp`, `RenderPipeline.cpp`

## Goal

让 Pass 通过具名语义组合，避免 RenderPipeline 保存和连接全部 RG Handle。

## Implementation

- `RenderGraphBlackboard` 发布 `SceneColor`、`SceneDepth`、`ShadowDepth`、GBuffer、Swapchain 等语义。
- Shadow、Forward、GBuffer、Deferred Lighting、Transparent、UI 图声明迁移为独立 Pass Module。
- RenderPipeline 只导入物理资源、选择 Pipeline 组合并提供执行回调。
- Blackboard 每帧随 Graph Clear 清空，禁止逻辑 Handle 跨帧泄漏。

## Design Reason

该边界接近 Unreal Render Dependency Graph 的参数传递方式：Pass 依赖显式资源语义，而不是修改 Renderer 的全局成员状态。

## Verification

Forward 与 Deferred 组合均使用同一 Blackboard 和模块入口运行。
