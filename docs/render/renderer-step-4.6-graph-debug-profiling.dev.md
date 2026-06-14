# Renderer Step 4.6：Graph 调试与性能查询

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderGraph.hpp`, `RenderGraph.cpp`, `RenderDiagnostics.hpp`, `RenderStatisticsPanel.cpp`

## Goal

让一帧 RenderGraph 的执行顺序、依赖、生命周期、Barrier 和耗时可观察。

## Implementation

- RenderGraph 生成 DAG、资源生命周期、Barrier、编译错误和文本 Dump 快照。
- 每个 Pass 记录 CPU 命令录制耗时。
- Vulkan 为每帧创建 Timestamp Query Pool；Fence 完成后读取每 Pass GPU 时间。
- Editor Render Statistics 面板展示 Pass DAG、CPU/GPU 时间、资源生命周期和 Barrier。

## Design Reason

复杂渲染问题必须先定位到具体 Pass 和资源状态，不能只依赖 Vulkan Validation 的底层句柄报告。

## Verification

Graph Dump 单元测试通过；Forward 与 Deferred 运行可连续完成多帧 Timestamp 查询。
