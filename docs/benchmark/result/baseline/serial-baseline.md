# Phase 0 Serial Renderer Baseline

## Result

表中使用三次运行按 Frame p50 排序后的中位运行，不删除任何原始帧；完整三次数据见相邻 JSON。

| Scene | Representative run | Frame p50/p95/p99 ms | Engine Tick p50/p95 ms | RenderGraph CPU p50 ms | GPU p50/p95 ms |
|---|---|---:|---:|---:|---:|
| instance-1k | run2 | 0.610 / 0.924 / 1.248 | 0.609 / 0.921 | 0.020 | 0.048 / 0.048 |
| instance-10k | run3 | 4.131 / 5.874 / 7.492 | 4.128 / 5.870 | 0.038 | 0.057 / 0.102 |
| instance-50k | run3 | 38.822 / 48.942 / 65.689 | 38.816 / 48.917 | 0.061 | 0.059 / 0.061 |

三个场景的 GPU missing sample 均为 0；Scene Hash 分别为 `103544098031816355`、`4618534146860686435`、`10543704414573488015`。

## Counters

代表帧中，1K/10K/50K 的 `visible / culled` 分别为 `30/970`、`213/9787`、`999/49001`。三者都形成 1 个实例 Batch、2 个 Draw；instance count 包含主场景实例和全屏输出 Draw。

## Analysis

- 50K 的 Engine Tick p50 几乎等于 Frame p50，而 RenderGraph 录制和 GPU 均低于 0.1 ms，当前明确是 CPU bound。
- 成本随总对象数增长远快于可见对象和 Draw，首要调查区是 Gameplay Tick、RenderSubsystem 全量同步、RenderWorld Snapshot 拷贝与串行 Frustum Test，而不是继续压缩 Draw Call。
- 1K/10K 跨运行相对波动较大，原因是绝对时间短、OS 调度噪声占比高；后续 Profiler 阶段应增加 CPU zone、线程固定/优先级实验和跨运行统计，不能用单次最好值声明收益。
- 100K 已完成 `3 warmup + 5 sample` smoke，未纳入正式 1000 帧矩阵，因此不对其性能作结论。

## Next Measurement Gate

Phase 1 Profiler 必须先把 Engine Tick 拆成 Gameplay、Render Bridge/Proxy Sync、Snapshot、Visibility、Queue Build、Instance Upload 和 RenderGraph Execute；Phase 2 Job System 与 Phase 3 Parallel Renderer 都复用本目录的场景、Hash 和输出 Schema。
