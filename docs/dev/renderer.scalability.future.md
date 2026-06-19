# Renderer Scalability 旗舰路线

## Metadata

- Date: 2026-06-19
- Status: Future Plan
- Depends on: Profiler, Job System
- Priority: P1

## Goal

把已有 Renderer 深度集中到 **Visibility 与 Submission Scalability**：同一份 RenderWorld 数据先完成多线程 CPU 路径，再完成 GPU-driven Culling 与 Indirect Draw，并使用统一 Profiler 和 Benchmark 比较。

这比继续增加独立画面效果更能展示：数据布局、并发拆分、RenderGraph、Compute、Vulkan 同步、回退路径和性能工程。

## 当前基础

- RenderWorld Snapshot 已隔离 Gameplay 指针。
- CPU Frustum Culling 遍历 `snapshot.objects`。
- Render Packet 生成、`stable_sort`、Batch 和 Instancing 已完成。
- RHI 已有 Compute Pipeline、Dispatch、Storage Buffer 和 Indirect Draw。
- RenderGraph 已表达 Compute/Graphics Buffer Barrier。

当前瓶颈候选：

- Render Bridge 对 Proxy 串行同步并复制 Snapshot。
- Visibility 对所有对象串行测试。
- Packet 构建和四条 Queue 排序串行。
- GPU 仍依赖 CPU 产出的可见对象和 Draw 提交结构。

候选必须由 Profiler 和压力场景确认后才能称为瓶颈。

## Stage 1：数据布局与串行 Oracle

- 为静态 Instance 建立连续 `RenderInstanceData`，减少遍历大型 Proxy 的无关字段。
- 分离 Static、Skinned、Transparent；GPU-driven 首版只处理 Static Opaque。
- 为 Visible Set、Packet 和 Batch 生成稳定 Hash。
- 保留现有串行路径作为正确性 Oracle 和低对象数回退。

**交付：** 不改变画面的数据布局重构，以及串行前后 Cache/Frame 数据。

## Stage 2：Job System 驱动的 CPU Path

- 按固定 Chunk 并行 Frustum Culling。
- 每个 Worker 写局部 Visible List，完成后按 Chunk Index 确定性合并。
- 并行 Packet Generation；禁止多个 Worker 争用一个 Vector。
- Opaque Sort 可从现有 `stable_sort` 过渡到可验证的并行 Key Sort/Radix Sort。
- Transparent Queue 保留稳定远到近语义，不为并行牺牲正确性。

**交付：** Worker 数扩展曲线、串行/并行 Visible Set 与 Packet Hash 一致。

## Stage 3：GPU-driven Frustum Culling

数据流：

```text
RenderWorld Static Instances
    -> Instance SSBO
    -> Compute Frustum Culling
    -> Visible Instance / Draw Group Compaction
    -> Indirect Argument Buffer
    -> DrawIndexedIndirect
```

实现步骤：

1. 定义后端无关 `GPUInstanceData` 和 Draw Group Key。
2. 使用 Reflection 驱动 Compute Shader 资源布局，不新增手写 Binding。
3. RenderGraph 导入/创建 Instance、Visible、Counter、Indirect Buffer。
4. Compute Pass 写 Storage/Indirect Buffer，Graphics Pass 以 `IndirectArgument` 状态读取。
5. RHI 暴露必要 Feature Query；不支持 Indirect Count 时使用固定 Draw Count 回退。
6. 每隔固定帧 Debug Readback GPU Visible Count，与 CPU Oracle 抽样比较。
7. Editor 提供 `Serial CPU / Job CPU / GPU Driven` 切换和统计。

## Stage 4：可选 Hi-Z Occlusion

只有 GPU Frustum 路径稳定且数据证明 Overdraw/不可见对象仍是主要问题后再实现：

- Depth Pyramid RenderGraph Pass。
- Previous-frame Hi-Z 读取与 Camera Cut 失效策略。
- Conservative Bounds Test 和可调 Bias。
- Occlusion Result 统计与 Debug Visualization。

Hi-Z 是 Stretch，不得拖延基础 GPU-driven 路径的报告和演示。

## 同步与资源约束

- Compute 到 Indirect Draw 的状态转换只能通过 RenderGraph/RHI 表达。
- 不在 RenderPipeline 中写 Vulkan Handle 或直接调用 `vkCmd*`。
- GPU Buffer 使用 Frame-safe 生命周期；Resize 和 Scene 切换不得销毁 In-flight 资源。
- Worker 不共享 Vulkan Command Pool；并行命令录制是后续独立增量。
- Upload 与 GPU Readback 不允许每帧 `WaitIdle()`。

## 验收矩阵

| 场景 | 串行 CPU | Job CPU | GPU Driven |
| --- | --- | --- | --- |
| 1K 静态对象 | 正确性与低负载开销 | 调度开销是否值得 | Dispatch 开销是否值得 |
| 10K 静态对象 | 基线 | Worker 扩展 | GPU Culling 收益 |
| 50K/100K 静态对象 | CPU 压力 | 扩展上限 | Submission 扩展性 |
| 5K 动态对象 | Snapshot/Upload 压力 | Job 数据准备 | Buffer 更新成本 |
| 高遮挡场景 | Frustum 基线 | 同左 | Hi-Z Stretch 对比 |

记录 p50/p95/p99 CPU Frame、Render Preparation、GPU Frame、Visible Count、Draw/Indirect Count、上传字节和 Profiler 开销。

## 后续候选优先级

完成本路线后再根据数据选择：

1. Clustered/Forward+ Lighting，适合展示 Compute + Light List。
2. RenderGraph Transient Memory Aliasing，适合展示资源生命周期和显存优化。
3. Secondary Command Buffer 并行录制，适合展示 Vulkan 多线程约束。
4. Async Compute，只有存在可重叠 Pass 和独立 Queue 时实施。

不得同时启动以上四项。

