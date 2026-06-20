# ChikaEngine Portfolio Roadmap - Minimal Steps

## Purpose

本目录把 `docs/dev/*.future.md` 拆成可独立实现、测试、审查和回滚的最小步骤。Game Runtime Phase 2-4 保持暂停；本路线只覆盖 Benchmark、Profiler、Job System、Renderer Scalability 和作品集交付。

## Execution Order

| Phase | Theme | Required Gate |
| --- | --- | --- |
| 0 | Reproducible benchmark baseline | 能稳定复现并保存串行 CPU 基线 |
| 1 | Unified CPU/GPU Profiler | Timeline、GPU Frame 对齐、Trace 导出和开销测试通过 |
| 2 | Job System / Task Graph | 并发正确性、Wait-help、Shutdown 和真实 Asset 接入通过 |
| 3 | Parallel CPU Renderer | 串行与并行结果一致，并有 Worker 扩展曲线 |
| 4 | GPU-driven Visibility | Compute Culling + Indirect Draw 正确且有 CPU/GPU 对照 |
| 5 | Optional renderer depth | 每次只选择一个由数据支持的 Stretch Step |
| 6 | Benchmark regression and portfolio | 原始数据、报告、Capture 和复现命令齐全 |

## Current Status

- Phase 0 completed on 2026-06-19: independent benchmark, deterministic workloads, delayed GPU correlation and the serial baseline are implemented.
- Phase 1 completed on 2026-06-19: unified CPU/GPU profiler, editor timeline, Perfetto export and overhead gate are implemented.
- Phase 2 completed on 2026-06-20: engine-owned Job System, dependencies, wait-help, profiler, Asset workload and read-only Renderer integration are implemented.
- Raw results: `../results/baseline/`, `../results/profiler/`, `../results/jobs/`.
- Next action: Step 3.1 Render serial oracle. Phase 3 must keep ResourceManager and RHI access off worker threads.

## Step Index

### Phase 0 - Benchmark Baseline

| Step | Deliverable |
| --- | --- |
| [0.1](0.1-benchmark-contract.future.md) | Benchmark target、CLI 和结果 Schema |
| [0.2](0.2-benchmark-scenes.future.md) | 确定性 1K-100K 压力场景 |
| [0.3](0.3-benchmark-runner.future.md) | Warmup、采样和 percentile runner |
| [0.4](0.4-serial-baseline.future.md) | 优化前串行 Renderer 报告 |

### Phase 1 - Unified Profiler

| Step | Deliverable |
| --- | --- |
| [1.1](1.1-profiler-event-contract.future.md) | Event、Clock、Name contract |
| [1.2](1.2-profiler-thread-buffer.future.md) | Thread registry 与 per-thread buffer |
| [1.3](1.3-profiler-instrumentation.future.md) | RAII Zone 和 instrumentation macros |
| [1.4](1.4-profiler-frame-history.future.md) | Frame aggregation、history 和 hitch capture |
| [1.5](1.5-profiler-engine-zones.future.md) | Engine/Renderer CPU zones |
| [1.6](1.6-profiler-gpu-correlation.future.md) | Frames-in-flight GPU timing 对齐 |
| [1.7](1.7-profiler-editor-timeline.future.md) | Editor Timeline 与 hotspot model |
| [1.8](1.8-profiler-trace-gate.future.md) | Perfetto export 与开销 Gate |

### Phase 2 - Job System

| Step | Deliverable |
| --- | --- |
| [2.1](2.1-job-module-lifecycle.future.md) | ChikaJobs 模块和 Worker 生命周期 |
| [2.2](2.2-job-storage-handle.future.md) | Job pool、状态和 generation handle |
| [2.3](2.3-job-worker-queues.future.md) | Injection/local queue、steal、sleep/wake |
| [2.4](2.4-job-dependencies.future.md) | Completion counter 和 dependencies |
| [2.5](2.5-job-wait-shutdown.future.md) | Wait-help、Drain 和 Cancel |
| [2.6](2.6-job-parallel-for.future.md) | ParallelFor 与确定性合并 |
| [2.7](2.7-job-profiler.future.md) | Scheduler profiling |
| [2.8](2.8-job-asset-integration.future.md) | AssetManager 真实工作负载接入 |

### Phase 3 - Parallel CPU Renderer

| Step | Deliverable |
| --- | --- |
| [3.1](3.1-render-serial-oracle.future.md) | Visible/Packet/Batch correctness oracle |
| [3.2](3.2-render-instance-layout.future.md) | Data-oriented static instance view |
| [3.3](3.3-render-parallel-visibility.future.md) | Parallel Visibility |
| [3.4](3.4-render-parallel-packets.future.md) | Parallel Packet Generation |
| [3.5](3.5-render-parallel-sort.future.md) | Parallel opaque sort 与 batch |
| [3.6](3.6-render-cpu-scaling-gate.future.md) | Worker scaling Gate |

### Phase 4 - GPU-driven Renderer

| Step | Deliverable |
| --- | --- |
| [4.1](4.1-gpu-capability-fallback.future.md) | RHI capabilities 和 fallback contract |
| [4.2](4.2-gpu-instance-data.future.md) | GPU instance/draw group layout |
| [4.3](4.3-gpu-culling-shader.future.md) | Reflection-driven Compute pipeline |
| [4.4](4.4-gpu-rendergraph-flow.future.md) | RenderGraph buffer flow/barriers |
| [4.5](4.5-gpu-cull-compaction.future.md) | GPU frustum culling/compaction |
| [4.6](4.6-gpu-indirect-submission.future.md) | Indirect argument 与 submission |
| [4.7](4.7-gpu-validation-debug.future.md) | CPU/GPU oracle、readback 和 debug |
| [4.8](4.8-gpu-performance-gate.future.md) | GPU-driven correctness/performance Gate |

### Phase 5 - Optional Renderer Depth

| Step | Trigger |
| --- | --- |
| [5.1](5.1-hiz-depth-pyramid.future.md) | Occlusion 数据支持 Hi-Z |
| [5.2](5.2-hiz-occlusion.future.md) | Depth Pyramid 稳定后实现 Occlusion |
| [5.3](5.3-clustered-lighting.future.md) | Many-light bottleneck |
| [5.4](5.4-secondary-command-recording.future.md) | CPU command recording bottleneck |
| [5.5](5.5-transient-memory-aliasing.future.md) | Transient peak memory bottleneck |
| [5.6](5.6-async-compute.future.md) | 存在独立 Queue 和可证明 overlap |

### Phase 6 - Regression and Portfolio

| Step | Deliverable |
| --- | --- |
| [6.1](6.1-benchmark-report-automation.future.md) | Benchmark matrix 和自动报告 |
| [6.2](6.2-regression-gates.future.md) | 正确性/性能回归策略 |
| [6.3](6.3-portfolio-artifacts.future.md) | README、报告、Trace、Capture、视频 |

## Step Contract

每个步骤必须满足：

1. 只承担文档声明的单一职责。
2. 公共 API 在实现前明确所有权、线程、生命周期和失败语义。
3. 新逻辑具有单元测试；跨模块路径增加最小集成测试。
4. 性能步骤保留串行或旧路径作为正确性 Oracle，直到 Gate 通过。
5. 不通过 `WaitIdle()`、全局大锁、静默降级或删除检查来伪造性能结果。
6. 完成后更新对应文档的 Status、实际文件、验证命令、数据和剩余限制。

## Common Verification

- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- Release Benchmark build and repeatable benchmark command
- Vulkan Validation run for correctness changes
- `git diff --check`

## File Naming

- `0.x`: Benchmark foundation
- `1.x`: Profiler
- `2.x`: Job System
- `3.x`: Parallel CPU Renderer
- `4.x`: GPU-driven Renderer
- `5.x`: Optional renderer depth
- `6.x`: Regression and portfolio delivery

## Boundary Rules

- Profiler Runtime data never depends on ImGui.
- Jobs never own Gameplay, Asset, Render or Vulkan objects.
- Worker tasks cannot access a shared Vulkan Command Pool.
- RenderGraph/RHI remain the only GPU resource-state and command boundary.
- Benchmark owns experiment orchestration, not Engine behavior.
- Stretch work cannot begin before Phase 4 Gate passes.
