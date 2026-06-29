# Renderer Step 3.4：稳定排序与状态缓存

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderQueue.cpp`, `RenderPipeline.cpp`, `RenderBaselineTests.cpp`

## Goal

让 Draw 顺序由渲染状态与透明深度决定，而不是由 GameObject 遍历顺序决定。

## Implementation

- Opaque 使用结构化稳定 Sort Key：`Pipeline -> Material -> Mesh -> RenderObjectHandle`。
- Transparent 首先按 View Depth 从远到近排序，再用资源和对象身份消除不稳定顺序。
- `DrawRenderQueue` 在单个 Queue 内缓存当前 Pipeline、Mesh、Material 和骨骼 Buffer；只有状态变化时才重新绑定。
- 结构化 Key 暂不压缩为 64-bit，避免资源 Handle 位宽增长时产生隐式截断。

## Design Reason

这对应 Unreal Mesh Draw Command 的状态排序目标：先获得确定且可测量的状态聚类，再形成 Batch。稳定对象 Handle 作为最终比较项，也便于复现渲染问题。

## Verification

- 单元测试覆盖 Opaque 状态聚类、Transparent 反向深度排序和重复构建 Queue。
- Frame Statistics 持续记录 Pipeline Bind 与 Descriptor Update。

## Remaining Boundary

当前 Descriptor 缓存仅限一次 Queue 录制；后续 Material Descriptor Cache 和 Pipeline State Cache 可扩大复用范围。
