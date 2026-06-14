# Renderer Debug Visibility：Frustum 与 AABB 可视化

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `Gizmo.hpp`, `RenderDebugVisualization.hpp`, `RenderDebugVisualization.cpp`, `ViewportPanel.cpp`

## Goal

在编辑器 Viewport 中手动开启或关闭 RenderWorld AABB 与 View/Light Frustum 可视化，直接检查 Phase 3 的 Bounds 和 Visibility 结果。

## Implementation

- `Debug::Gizmo` 增加世界空间 `DrawWireAABB` 与 `DrawFrustum` 基元。
- `RenderDebugVisualization` 只读取不可变 RenderWorld Snapshot，并生成编辑器调试线段。
- AABB 使用与主视图 Visibility 相同的 Flag、Layer 和 Frustum 规则：
  - 绿色：主视图可见。
  - 红色：被隐藏、Layer 过滤或视锥剔除。
- Frustum 使用不同颜色区分 Primary View、Secondary View 和投影阴影的 Light。
- Viewport 工具栏提供 `AABB` 与 `Frustum` 两个独立 Checkbox，默认关闭。

## Design Reason

调试线框不进入正式 RenderQueue、Batch、Shader 或 RHI Pipeline，因此不会改变 Draw、Instance 等正式帧统计。Renderer 只负责从渲染侧 Snapshot 提供真实 Bounds 与 Frustum，Editor 负责显示和交互开关。

## Verification

- 单元测试验证两个 AABB、一个 View Frustum 和一个 Light Frustum 共生成 48 条线，并验证可见/剔除颜色。
- 关闭全部选项时不生成调试线段。

## Remaining Boundary

当前 Gizmo 由 ImGui Overlay 绘制，不参与场景深度测试；Primary View Frustum 从自身视口观察时会与视口边缘重叠。后续可增加正式 Debug Draw RHI Pass、线段裁剪、标签、单对象过滤和独立 Scene View。
