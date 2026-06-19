# Benchmark 与作品集交付规划

## Metadata

- Date: 2026-06-19
- Status: Future Plan
- Depends on: Profiler, Job System, Renderer Scalability
- Priority: P0 for final delivery

## Goal

把技术实现转化为招聘方可验证的证据。Benchmark 不是最后补一个 FPS 数字，而是从 Phase A 开始约束设计、回归和最终表达。

## Benchmark 场景

### Scene A：Instance Throughput

- 1K、10K、50K、100K 相同 Mesh/Material。
- 验证 Culling、Batch、Instance、Indirect Submission。

### Scene B：State Diversity

- 多 Mesh、多 Material、不同 Pipeline Key。
- 验证 Packet Sort、Batch Fragmentation 和 Descriptor/Pipeline Bind。

### Scene C：Dynamic Transform

- 5K 持续更新 Transform 的对象。
- 验证 Render Bridge、Snapshot、上传字节和 Job 数据准备。

### Scene D：Occlusion Stress（Stretch）

- 大量被墙体遮挡的对象和移动 Camera。
- 验证 Hi-Z 收益、滞后和 Camera Cut 行为。

## 测量协议

- Release Build，记录 Commit、Compiler、CPU、GPU、Driver、分辨率。
- VSync 关闭，固定 Camera Path 和固定随机种子。
- 预热至少 300 帧，采样至少 1000 帧。
- 报告 Frame Time 的 p50/p95/p99，不只报告平均 FPS。
- CPU 与 GPU 时间分开；明确当前是 CPU Bound 还是 GPU Bound。
- 每种路径至少重复 3 次，并保留原始 JSON/CSV。
- 性能 Capture 不运行 Validation Layer；正确性 Capture 单独开启 Validation。

对比配置：

- Serial CPU Reference。
- Job CPU：1/2/4/8 Worker。
- GPU-driven。
- Profiler Off/On。

## 正确性协议

- CPU 与 Job 路径比较 Visible Set、Packet、Batch 稳定 Hash。
- GPU 路径定期 Readback Visible Count，并抽样比较 CPU Oracle。
- 固定 Camera 截图进行容差 Image Diff。
- Vulkan Validation 不出现 Error；资源关闭路径运行多次。
- 测试低对象数，证明并行/GPU 路径不会因固定开销全面回退。

## 自动化交付

建议新增独立 Benchmark 可执行程序，而不是依赖 Editor 手动操作：

```text
ChikaBenchmark
  --scene instance-10k
  --mode serial|jobs|gpu
  --workers 1|2|4|8
  --warmup 300
  --frames 1000
  --output results/run.json
```

结果 Schema 至少包含：

- Build/Hardware Metadata。
- p50/p95/p99 Frame、CPU、GPU 时间。
- 各 Profiler Zone 聚合。
- Draw、Batch、Instance、Visible/Culled、Indirect Count。
- Job Utilization、Queue Wait、Steal Count。
- Upload Bytes 和 Profiler Dropped Event Count。

## 回归门禁

- 正确性测试必须硬失败。
- 性能回归先生成报告，不使用单次运行硬失败。
- CI 有稳定专用机器后，才对 p95 设置宽松阈值。
- 性能改动必须附带 Before/After 原始结果，不接受只贴截图。

## 作品集页面结构

1. **Problem**：串行 Visibility/Submission 在大场景中的瓶颈。
2. **Architecture**：Profiler、Task Graph、RenderWorld、RenderGraph、RHI 的边界图。
3. **Engineering Decisions**：Wait-help、局部输出确定性合并、GPU/CPU Oracle、Frames In Flight Query。
4. **Results**：扩展曲线和 p95 Frame Time，而非单个最高 FPS。
5. **Trade-offs**：低对象数调度开销、GPU Readback 延迟、功能回退。
6. **Remaining Work**：Hi-Z、Clustered Lighting、Memory Aliasing 等真实限制。

## 最终材料

- 60-90 秒视频：Profiler Timeline -> Worker 扩展 -> GPU-driven 切换 -> 数据图表。
- 一张完整架构图和一张 Job 状态图。
- 一份 3-5 页性能报告。
- 一个可打开的 Chrome/Perfetto Trace。
- 一个 RenderDoc Capture，Pass/资源具有可读 Debug Name。
- README 中提供复现实验命令。

## 禁止做法

- 不只展示 Debug Build FPS。
- 不隐藏低对象数下 Job/GPU 固定开销。
- 不把不同分辨率、Camera 或画质的结果放在同一对比图。
- 不使用“提升数倍”而不附测试条件和原始数据。
- 不为了漂亮数字删除 CPU Reference 或关闭正确性检查。

