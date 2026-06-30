# ChikaEngine 求职作品集技术主线

## Metadata

- Date: 2026-06-19
- Status: Future Plan
- Scope: Renderer / Profiler / Job System / Benchmark
- Runtime Roadmap: Paused after Phase 1

## 定位

作品集主线调整为：

> **一个可测量、任务图驱动、具备 CPU/GPU 可扩展渲染路径的 C++20 Vulkan Engine。**

重点不是继续堆叠渲染效果数量，而是证明能够完成复杂系统的边界设计、性能定位、并发实现、后端抽象、正确性验证和量化优化。

这条主线同时覆盖三类岗位能力：

| 岗位方向 | 可展示能力 |
| --- | --- |
| Rendering Engineer | RenderGraph、RHI、GPU Timestamp、Compute Culling、Indirect Draw、同步与资源状态 |
| Engine Engineer | Job System、模块生命周期、数据所有权、Profiler、资源与线程边界 |
| C++ Engineer | 并发正确性、RAII、Handle/Pool、缓存友好数据布局、测试和基准方法 |

## 当前基础

已经具备：

- Shader Reflection 驱动 Descriptor、Pipeline Layout、Vertex Layout 和 Material Offset。
- Gameplay 与 Renderer 之间通过 RenderWorld / Snapshot 解耦。
- RenderGraph Pass DAG、资源生命周期、Barrier、Pass Culling 和 Blackboard。
- Vulkan RHI、GPU Pass Timestamp、CPU Pass 录制耗时和基础 Frame Statistics。
- Frustum Culling、Render Packet、排序、Batch、Instancing、Forward/Deferred PBR。

主要缺口：

- Profiler 只覆盖 RenderGraph 局部，没有跨线程 CPU Timeline、Frame History 和 Trace 导出。
- `Core/threads` 中的 `TSQueue` 为空，资产异步加载仍使用零散 `std::async`。
- RenderSubsystem 同步、Visibility、Packet 构建和排序仍以串行为主。
- 缺少可重复 Benchmark，现有优化无法回答“快了多少、为什么、代价是什么”。

## 推荐路线

### Phase A：冻结基线与性能问题

预计 1 周。

- 建立 1K、10K、50K/100K 对象压力场景。
- 固定分辨率、关闭 VSync，记录硬件、构建类型和采样方法。
- 保存当前单线程 CPU 路径的 p50/p95/p99 Frame Time、主线程时间、GPU 时间和 Draw/Batch 数据。
- 输出第一版性能报告，禁止先写并发代码后寻找理由。

**Gate：** 能稳定复现实测瓶颈，并区分 Game Thread、Render Preparation、GPU Bound。

### Phase B：建立全引擎 Profiler

预计 2 周，详见 `profiler.timeline.future.md`。

- 实现低开销 CPU Zone、Counter、Frame Marker 和线程元数据。
- 聚合现有 Vulkan Timestamp，正确处理 Frames In Flight 延迟。
- Editor 显示多线程 Timeline、Frame History、热点排序。
- 支持 Chrome Trace JSON 导出，便于脱离 Editor 展示和复核。

**Gate：** 后续每项优化都能在同一 Capture 中看到 CPU/GPU 因果链。

### Phase C：实现 Job System 与 Task Graph

预计 3 周，详见 `jobsystem.taskgraph.future.md`。

- Worker Pool、任务存储、Work Stealing、Job Handle/Counter、依赖与 Wait-help。
- 完整启动、Drain、Cancel、Shutdown 语义。
- Profiler 自动记录 Job、Worker、Steal 和等待时间。
- 用压力测试、依赖图测试和引擎关闭测试证明正确性。

**Gate：** Job System 不是孤立 Demo，至少驱动一个资产任务和一个 Renderer 任务。

### Phase D：Renderer CPU 可扩展化

预计 2 周，详见 `renderer.scalability.future.md`。

- 并行化纯数据阶段：Visibility、Packet Generation、Queue Sort/Batch。
- 每个 Worker 写局部输出，主线程确定性合并，禁止共享 `push_back` 热锁。
- 保留串行参考路径，逐帧比较 Visible Set、Packet Hash 和最终图像。

**Gate：** 10K/50K 场景展示 1/2/4/8 Worker 的扩展曲线，而不只展示最高 FPS。

### Phase E：旗舰功能 GPU-driven Visibility

预计 4 周。

- GPU Instance Buffer 与 Draw Group。
- RenderGraph Compute Pass 完成 Frustum Culling 和可见列表压缩。
- 生成 Indirect Arguments，通过 RHI 执行 Draw Indexed Indirect。
- CPU 路径继续作为回退与正确性 Oracle。
- Hi-Z Occlusion、Indirect Count、Clustered Lighting 只作为 Stretch Goal。

**Gate：** CPU、Job System、GPU-driven 三条路径在同一场景下有正确性与性能对照。

### Phase F：作品集收尾

预计 1 周，详见 `benchmark.presentation.future.md`。

- 自动 Benchmark、结果文件、架构图、性能报告、RenderDoc Capture。
- 录制 60-90 秒技术演示，不录普通场景漫游。
- README 使用“问题 -> 设计 -> 取舍 -> 数据 -> 限制”结构。

## Must-have 与 Stretch

Must-have：

- CPU/GPU Timeline Profiler。
- 可关闭、可验证、可正确退出的 Job System。
- Renderer 至少一个真实并行工作负载。
- 可重复 Benchmark 和 Before/After 数据。
- GPU-driven Frustum Culling + Indirect Draw。

Stretch：

- Hi-Z Occlusion Culling。
- Clustered/Forward+ Light Culling。
- Vulkan Secondary Command Buffer 并行录制。
- RenderGraph Transient Memory Aliasing。
- Async Compute Queue。

Stretch 不得阻塞 Must-have 的测试、报告和演示。

## 明确暂停

- 暂停 Game Runtime Phase 2-4、Cook 和 Package。
- 暂停继续增加零散后处理、更多光源类型和 Editor 小工具。
- 暂停脚本系统扩展和完整 Gameplay 产品化。
- 不以“支持功能数量”作为完成标准，以可测量的系统闭环作为完成标准。

## 最终简历表述目标

最终应能用数据填写，而不是预先编造数字：

> Designed a task-graph-driven Vulkan renderer with cross-thread CPU/GPU profiling, parallel visibility and packet generation, and GPU-driven indirect submission; validated scaling and frame-time improvements using reproducible 1K-100K object benchmarks.

