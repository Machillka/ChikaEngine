# Renderer Step 3.5：Render Batch

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderQueue.hpp`, `RenderQueue.cpp`, `RenderPipeline.cpp`, `RenderDiagnostics.hpp`

## Goal

将排序后共享完整绘制状态的连续 Packet 合并为稳定提交单元。

## Implementation

- `RenderBatch` 保存 Pass、Pipeline、Material、Mesh、Packet 范围、First Instance 和实例化标记。
- Batch Builder 仅合并相同 Pass、Pipeline、Material、Mesh 且允许实例化的连续静态 Packet。
- 动态、蒙皮和透明对象保持独立 Batch，避免共享错误的 Object Data。
- Pass 录制改为逐 Batch 绑定与 Draw，并暴露 Packet、Batch、Instanced Batch 统计。

## Design Reason

排序只减少相邻状态差异，Batch 才是可直接录制和实例化的提交单位。它对应成熟引擎中由多个可见 Primitive 形成共享 Draw State 的中间层。

## Verification

- 单元测试验证相同静态状态合并为一个 Batch，不同状态保持分离。
- Play Mode Forward 基准场景为 `12 packets / 4 batches`。

## Remaining Boundary

当前 Batch 每帧在 CPU 重建；后续可为稳定静态场景缓存静态 Draw Command，并仅增量更新变化对象。
