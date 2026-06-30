# Unified CPU/GPU Profiler 规划

## Metadata

- Date: 2026-06-19
- Status: Phase 1 Complete (2026-06-19)
- Depends on: Existing RenderGraph GPU Timestamp
- Priority: P0

## Goal

把现有“RenderGraph 面板统计”升级成全引擎 Instrumentation Profiler，使性能问题能够定位到线程、Job、Subsystem、Render Pass 和 GPU Pass，而不是依赖手工日志或单个平均 FPS。

## 当前边界

已有：

- RenderGraph Pass CPU 录制耗时。
- Vulkan Timestamp Query 和每 Pass GPU 时间。
- Draw、Instance、Pipeline Bind、Descriptor Write、Visibility、Batch 统计。
- Editor Render Statistics 面板。

Phase 1 已完成：

- Main/任意注册线程 CPU Timeline、300 帧 History、p50/p95/p99、卡顿和 pin。
- Vulkan GPU pass 延迟回填、Perfetto/Chrome Trace、Profiler 自身开销基准。
- Engine、Gameplay、RenderWorld、Renderer 和 RenderGraph 阶段级 CPU zones/counters。

后续仍缺少：

- Job 依赖、排队、执行、等待和 Work Steal 可视化；这属于 Phase 2 Job System 接入。
- CPU/GPU 时钟绝对校准、远程采集和统计采样 profiler；不属于 Phase 1 边界。

## 数据模型

首版事件：

- `FrameMark`：一帧边界。
- `ZoneBegin/ZoneEnd`：层级 CPU 区间。
- `Counter`：对象数、队列长度、内存和命令统计。
- `Instant`：资源重载、Job Steal、Pipeline Compile 等离散事件。
- `ThreadName`：稳定线程身份。

每条事件至少保存：

- 单调时钟 Timestamp。
- Thread ID。
- Interned Name ID，不在热路径复制字符串。
- Event Type 和小型 Payload。

## 架构

```text
Instrumentation Macro / RAII Scope
    -> Per-thread Event Buffer
    -> Frame Aggregator
    -> Capture Ring Buffer
    -> Editor Timeline / Statistics / Chrome Trace Export

Vulkan Timestamp N
    -> Fence completion N+k
    -> GPU Timing Resolver
    -> Frame Aggregator 对齐原始 Frame ID
```

关键约束：

- 热路径不分配堆内存，不使用全局事件锁。
- 每线程写自己的 Buffer；Aggregator 在帧边界读取已完成区段。
- GPU Query 不允许 `WaitIdle()`；必须读取已完成的旧帧。
- Disabled Build 下宏应被编译掉或接近零成本。
- Capture 数据是 Runtime/Core 能力，ImGui 只是消费者。

## 最小步骤

1. 定义 `ProfilerEvent`、`ProfilerClock`、`ProfilerSession` 和 RAII `ProfileScope`。
2. 实现线程注册、名称 Interning 和 Per-thread 固定容量 Event Buffer。
3. 增加 `CHIKA_PROFILE_SCOPE`、`CHIKA_PROFILE_FUNCTION`、`CHIKA_PROFILE_COUNTER`。
4. 接入 Application、Scene Tick、Physics、Animation、Render Bridge、Visibility、Queue Build、RenderGraph。
5. 将现有 GPU Timing 关联到原始 Frame ID，显示查询延迟而不是错配到当前帧。
6. 实现最近 300 帧历史、卡顿阈值和手动 Capture。
7. Editor 增加 Timeline、Thread Lanes、Zone Details、Top Hotspots。
8. 导出 Chrome Trace JSON，并附带 Engine/Build/Hardware Metadata。

## 测试与验收

- 嵌套 Zone 能恢复正确父子关系。
- 多线程并发写入不丢失完整事件；Buffer 溢出有计数而不是越界。
- 线程退出和 Engine Shutdown 后不保留悬空 Buffer。
- GPU Frame ID 在 2-3 Frames In Flight 下仍正确对齐。
- 10K 对象基准中：Disabled 开销目标低于 0.1%，Enabled 开销目标低于 2%。
- 导出的 Trace 能被 Chrome/Perfetto 打开并显示线程和 Zone。

百分比是验收目标，必须由 Benchmark 实测；未达到时报告真实结果和瓶颈。

## 作品集交付

- 一张 CPU/GPU 联合 Timeline 截图。
- 一个包含卡顿帧的 Trace 文件。
- 一页说明 GPU Timestamp 延迟解析与 Frame 对齐。
- 一组 Profiler On/Off 开销数据。

## 非目标

- 首版不实现统计采样 Profiler。
- 首版不 Hook 全局 Allocator。
- 首版不追求网络远程采集。
- 不让 Editor UI 参与采样或控制 Runtime 数据所有权。
