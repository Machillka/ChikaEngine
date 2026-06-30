# Renderer Step 3.2：Frustum Visibility

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderVisibility.hpp`, `RenderVisibility.cpp`, `RenderPipeline.cpp`, `RenderDiagnostics.hpp`

## Goal

在 Render Queue 构建前移除视锥外、隐藏和 Layer 不匹配对象。

## Implementation

- `ViewFrustum` 从 Vulkan `0..1` 深度范围的 ViewProjection 提取并归一化六个平面。
- AABB 使用中心到平面的有符号距离和投影半径进行保守相交测试。
- `BuildVisibility` 输出 `VisibilityResult`，只保存可见 Snapshot 对象指针，并统计 tested、visible、culled。
- 主视图与阴影视图分别构建可见集合；无效 Bounds 降级为可见，避免资产错误导致对象永久消失。

## Design Reason

类似 Unreal 的 View Visibility 阶段，空间判断位于 Scene 数据与 Draw Command 之间。这样排序、分类和批处理只处理真正可能产生 Draw 的 Proxy。

## Verification

- 单元测试覆盖视锥内、视锥外和隐藏对象。
- 基准场景固定放置一个远处 Box，运行统计为 `6 visible / 1 culled`，该对象不产生 Draw。

## Remaining Boundary

当前为线性 CPU Frustum Culling；后续需要接入空间层级、距离剔除、遮挡剔除和多 View 复用。
